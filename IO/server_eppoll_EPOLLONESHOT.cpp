/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2022-11-04 20:33:11
 * @LastEditors: baisichen
 * @Description: 测试EPOLLONESHOT事件
 * EPOLLONESHOT主要是为了让多线程情况下在任意时刻一个socket链接只会被一个线程处理。
 * 比如一个线程在读取完某个socket上的数据后开始处理数据，在数据处理的过程中该socket上又有新的数据可读，
 *  此时另外一个线程被唤醒来读这些新的数据，这就出现了两个线程同时操作一个socket的局面，会造成不可预期的错误
 * gcc -lpthread -o server server.cpp
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
#define BUFFER_SIZE 1024

struct  fds
{
    int epollfd;
    int sockfd;
};

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
void addfd(int epollfd, int fd, bool oneshot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot)
    {
        printf("oneshot\n");
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//重置fd上的事件，这样操作后，尽管fd上的EPOLLONESHOT事件被注册。但是操作系统仍然会触发fd上EPOLLIN事件，且触发一次
void reset_oneshot(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void * worker(void * arg) {
    //假设传进来的是一个fds
    int sockfd = ((fds*) arg)->sockfd;
    int epollfd = ((fds *)arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    /*循环读取sockfd上的数据，直到遇到EAGIN错误*/
    while(1) {
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        if (ret == 0) {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        } else if (ret < 0) {
            if (errno == EAGAIN) {
                /*这里如果没有reset的话同一个sockfd上将不会再出现EPOLLIN事件，即使客户端还在不停写
                能出现这种的前提是客户端发送数据间隔要比服务器处理的时间要长，可以触发一次EAGAIN事件，触发socket的非阻塞情况
                */

                reset_oneshot(epollfd, sockfd); 
                printf("read later\n");
                break;
            }
        } else {
            printf("get content: %s\n", buf);
            //休眠5s模拟数据处理过程
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
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
    int epollfd = epoll_create(5); //用来管理epoll的内核注册表的标识符
    assert(epollfd != -1);
    /*
    注意这里，监听socket listenfd上是不能注册EPOLLESHOT事件的，否则应用程序只能处理一个客户端连接(因为listenfd也是一个socket描述符)。
    因为后续的客户请求将不再触发listenfd上的EPOLLIN事件
    */
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
        //循环遍历就绪的事件
        for (int i=0; i< ret; i++) {
            int sockfd = events[i].data.fd;
            //这个描述符只用来处理客户端连接请求的
            if (sockfd == listenfd) {
                struct sockaddr_in cli_address;
                socklen_t cli_len = sizeof(cli_address);
                int connfd = accept(listenfd, (sockaddr*)&cli_address, &cli_len);
                addfd(epollfd, connfd, true); //这里之所以能直接加，是因为listenfd上如果没有就绪事件的话，是不会epoll通知的
            } else if (events[i].events & EPOLLIN) {//发生了EPOLLIN事件，因为新增或处理完毕都会重置EPOLLONESHOT,所以总会触发EPOLLIN事件
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd=epollfd;
                fds_for_new_worker.sockfd=sockfd;
                //新启动一个工作线程为sockfd服务
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            } else {
                printf("something else happened\n");
            }
        }
    }
    close(listenfd);
    return 0;
}