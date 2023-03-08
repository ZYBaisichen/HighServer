/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2022-11-07 12:12:16
 * @LastEditors: baisichen
 * @Description: 同时处理UDP和TCP请求. 本程序需要注意的是同一个端口监听两种报文，需要建立两个socket并分别绑定
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
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

//将文件描述符设置成非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    // int new_option = old_option & ~O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将文件描述符fd上的EPOLLIN|EPOLLET注册到epollfd指示的epoll内核事件表中
void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
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

    //创建TCP SOCKET, 并将其绑定到端口port上
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    //创建UDP socket, 并将其绑定到端口port上，注意这里的port和TCP是一个端口，只不过一个是处理TCP的socket，一个是处理UDP的socket
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpfd>0);
    ret = bind(udpfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    //注册TCP和UDP socket上的可读事件
    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    while (1)
    {
        //number为就绪的文件描述符个数，就绪的事件放在了events中
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0)
        {
            printf("epoll failure\n");
            break;
        }
        
        for (int i=0; i< number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in cli_address;
                socklen_t cli_len = sizeof(cli_address);
                int connfd  = accept(listenfd, (struct sockaddr*)&cli_address, &cli_len);
                addfd(epollfd, connfd);
            } else if (sockfd == udpfd) {
                //这里不用accept，因为是UDP的，所以直接接收请求就可以
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                struct sockaddr_in cli_address;
                socklen_t cli_len = sizeof(cli_address);

                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&cli_address, &cli_len);
                //UDP的回射服务器
                if (ret > 0) {
                    sendto(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&cli_address, cli_len);
                }
            } else if(events[i].events & EPOLLIN) {
                char buf[TCP_BUFFER_SIZE];
                while(1) {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE-1, 0);
                    if (ret < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            //非阻塞的，收到AGAIN信号，则先结束本次epoll_wait，等数据到来再触发epoll_wait
                            break;
                        }
                        close(sockfd);
                        break;
                    } else if (ret == 0) {
                        close(sockfd);
                    } else {
                        //回射数据
                        send(sockfd, buf, ret, 0);
                    }
                }
            } else {
                printf("something else happened\n");
            }
        }
    
    }
    close(listenfd);
    return 0;
}