/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2022-11-03 19:59:57
 * @LastEditors: baisichen
 * @Description: epoll的LT和ET模式差别
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <libgen.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1000

//将文件描述符设置成非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    // int new_option = old_option & ~O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中，参数enable_et指定是否对fd启用ET模式
void addfd(int epollfd, int fd, bool enable_et)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et)
    {
        printf("epollet\n");
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//LT模式的工作流程
void lt(epoll_event *events, int numbser, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < numbser; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in client_address;
            socklen_t client_addrlen = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlen);
            addfd(epollfd, connfd, false);//将获取到的读取数据的文件描述符添加到epollfd中，使用lt模式
        }
        else if (events[i].events & EPOLLIN)
        {

            /* 
                加上这两句代码就比较好理解了，lt模式下如果不做任何操作，刚才通过accept拿到的connfd上的数据就没有被读取
                因为connfd描述符是lt模式，所以内核事件表中一直有这个文件描述符，epoll_wait的时候就会一直将其视为就绪事件
                就会一直做处理，这里的numbser就是就绪的描述符个数
                printf("event trigger once\n");
                continue;
             */
            //只要socket读缓存中还有未读出的数据，这段代码就被触发
            printf("event trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0)
            {
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n", ret, buf);
        }
        else
        {
            printf("something else happened");
        }
    }
}

//ET模式的工作流程
void et(epoll_event *events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
            addfd(epollfd, connfd, true);
        }
        else if (events[i].events & EPOLLIN)
        {
            printf("event trigger once\n");
            while (1)
            {
                /*
                    加上下面这个break语句就很好理解了，假设什么都不做，在et模式下，内核会认为你已经处理完了connfd上的所有数据
                    下次epoll_wait的时候也不会再次通知就绪了
                    break;
                */
                memset(buf, '\0', BUFFER_SIZE);
                /*
                预想的这里如果sockfd设置为了阻塞的，会在recv上一直等待，但设置了阻塞模式，但需要客户端配合
                假设客户端发送数据时sleep了10秒，就能观察出来了，未完待续。。。。。
                */
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0); 
                printf("recv ret is:%d, errno:%d\n", ret, errno);
                if (ret < 0)
                {
                    //对于非阻塞IO，下面的条件成立标识数据已经全部读取完毕。此后epoll就能再次触发sockfd上的EPOLLIN事件，以驱动下一次读操作
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    {
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if (ret == 0)
                {
                    //这里并不是接收到了0条数据，而是对端关闭了连接，我们也关闭
                    printf("close cockfd\n");
                    close(sockfd);
                }
                else
                {
                    printf("get %d bytes of content:%s\n", ret, buf);
                }
            }
            /*
                将文件描述符变为阻塞的，两个客户端同时请求时的输出
                event trigger once
                get 5 bytes of content:123ab
                get 3 bytes of content:123
                seep end!
                这里睡了10秒才去处理的第二条就绪的fd，如果第一个connfd上数据特别多，
                很容易造成后来的文件描述符上的程序将会因为没有可读、可写事件到来而处于饥渴状态
                event trigger once
                get 5 bytes of content:123ab
                get 3 bytes of content:123
            */
            // sleep(10);
            // printf("seep end!");
        }
        else
        {
            printf("something else happened\n");
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s, ip port\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);

    while (1)
    {   
        //ret为就绪的文件描述符个数，就绪的事件放在了events中
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
        {
            printf("epoll failure\n");
            break;
        }
        //lt
        // lt(events, ret, epollfd, listenfd);
        //et
        et(events, ret, epollfd, listenfd);
    }
    close(listenfd);
    return 0;
}   