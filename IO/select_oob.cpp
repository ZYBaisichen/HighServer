/*** 
 * @Author: baisichen
 * @Date: 2022-10-28 20:02:00
 * @LastEditTime: 2022-10-28 20:46:29
 * @LastEditors: baisichen
 * @Description: select处理带外数据和正常数据
 * int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
/*
客户端发送"123""abc""123"，其中abc为oob带外数据
get 5 bytes of normal data: 123ab
get 1 bytes of oob data: c
get 3 bytes of normal data: 123
*/
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: %s ip, port\n", argc);
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
    assert(listenfd > 0);
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in cli_address;
    bzero(&cli_address, sizeof(cli_address));
    socklen_t cli_len = sizeof(cli_address);
    int connfd = accept(listenfd, (struct sockaddr *)&cli_address, &cli_len);
    if (connfd < 0)
    {
        printf("errno is %d\n", errno);
        close(connfd);
    }

    char buf[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while (1)
    {
        memset(buf, '\0', sizeof(buf));
        /*每次调用select前都要重新在read_fds和exceptioin_fds中设置文件描述符connfd，因为时间发生后，文件描述符集合将被内核修改*/
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0)
        {
            printf("select failed\n");
            break;
        }
        //对于可读事件，采用普通的recv函数读取数据
        if (FD_ISSET(connfd, &read_fds))
        {
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if (ret <= 0)
            {
                break;
            }
            printf("get %d bytes of normal data: %s\n", ret, buf);
        }
        //书上的代码没有考虑到oob为多个字符的情况，所以这里应该是if而不是else if
        memset(buf, '\0', sizeof(buf));
        if (FD_ISSET(connfd, &exception_fds))
        {
            //对于异常时间，采用MSG_OOB标志的recv函数读取带外数据
            ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
            if (ret <= 0)
            {
                break;
            }
            printf("get %d bytes of oob data: %s\n", ret, buf);
        }
    }
    close(connfd);
    close(listenfd);
    return 0;
}