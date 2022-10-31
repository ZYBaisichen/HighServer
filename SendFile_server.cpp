/*** 
 * @Author: baisichen
 * @Date: 2022-10-20 17:05:01
 * @LastEditTime: 2022-10-20 22:01:40
 * @LastEditors: baisichen
 * @Description: 零拷贝函数sendfile，头文件在<sys/sendfile.h>，原型为：
 *  ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count)
 * sendfile函数在两个文件描述符中间直接传递数据（完全在内核中操作），从而避免了内核缓冲区和用户缓冲区之间的数据拷贝，效率很高，这被称为零拷贝。
 *  out_fd为输出描述符，in_fd为输入描述符（通常为文件） ，offset指定从文件哪里开始，count设置从in_fd到out_fd传输多少字节数
 */
#include <sys/socket.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
int main(int argc, char *argv[])
{

    //简便起见文件通过参数传过来
    if (argc <= 3)
    {
        printf("Usage: %d param, ip, port, filename\n", argc);
    }
    //处理参数
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *file_name = argv[3];

    //打开文件获取文件描述符
    int filefd = open(file_name, O_RDONLY);
    assert(filefd > 0);
    struct stat file_stat;
    fstat(filefd, &file_stat);
    printf("file_name:%s, stat:%d", file_name, file_stat.st_size);

    //创建一个socket，返回socket描述符
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    //创建地址, 使用比较方便的sockaddr_in
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    //将sock地址绑定到socket上
    int ret = bind(sock, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    //listen，监听端口
    ret = listen(sock, 5);
    assert(ret != -1);

    //从链接队列里拿一个已经经过了三次握手的连接
    struct sockaddr_in client;
    socklen_t cli_len = sizeof(client);
    int connfd = accept(sock, (struct sockaddr *)&client, &cli_len);

    if (connfd < 0)
    {
        printf("error: %d", errno);
    }
    else
    {
        printf("st_size:%d", file_stat.st_size);
        sendfile(connfd, filefd, NULL, static_cast<size_t>(file_stat.st_size)); //返回感觉有点问题，不过这个
        close(connfd);
    }

    close(filefd);
    close(sock);

    return 0;
}