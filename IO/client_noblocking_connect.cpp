/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2022-11-06 11:46:29
 * @LastEditors: baisichen
 * @Description: 测试非阻塞connect
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

/*超时连接函数，参数分别是服务器IP地址、端口号和超时时间(毫秒)。函数成功是返回已经处于链接状态的socket，失败则返回-1*/
int unblock_connect(const char* ip, int port, int time) {
    int ret= 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, & address.sin_addr);
    address.sin_port = htons(port);
    
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int fdopt = setnonblocking(sockfd); //注意这里必须设置socket为非阻塞的，否则是出不来EINPROGRESS错误码的
    ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    printf("rrrrret:%d", ret);
    if (ret == 0) {
        //如果链接成功，则回复sockfd的属性，并立即返回之
        printf("connect with server immediately\n");
        fcntl(sockfd, F_SETFL, fdopt);
        return sockfd;
    } else if (errno != EINPROGRESS) {
        //如果链接没哟竌建立，那么只有当errno是EINPROGRESS时才标识链接还在进行，否则出错返回
        printf("unblock connect not support\n");
        return -1;
    }

    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &writefds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sockfd+1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0) {
        //select超时或出错，立即返回
        printf("connection time out\n");
        close(sockfd);
        return -1;
    }

    if (! FD_ISSET(sockfd, &writefds)) {
        printf("no events on sockfd found\n");
    }

    int error = 0;
    socklen_t length = sizeof(error);
    /*调用getsockopt来获取并清除sockfd上的错误*/
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
        printf("get socket option failed\n");
        close(sockfd);
        return -1;
    }
    //错误号不为0表示链接出错
    if (error != 0) {
        printf("connecttion failed afster select with the error:%d\n", error);
        close(sockfd);
        return -1;
    }
    //链接成功
    printf("connection ready after select with the socket:%d\n", sockfd);
    fcntl(sockfd, F_SETFL, fdopt); //重置配置
    return sockfd;
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

    int sockfd = unblock_connect(ip, port, 10);
    if (sockfd < 0) {
        return 1;
    }

    close(sockfd);
    return 0;
}