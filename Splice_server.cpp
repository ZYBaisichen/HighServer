/*** 
 * @Author: baisichen
 * @Date: 2022-10-13 20:06:37
 * @LastEditTime: 2022-10-21 10:46:10
 * @LastEditors: baisichen
 * @Description: 使用splice函数实现的回射服务器，将客户端发送的数据原样返回给客户端
 * splice函数：用于在两个文件描述符之间移动数据，也是零拷贝操作
 *      #include<fcntl.h>
 *      ssize splice(int fd_in, loff_t* off_in, int fd_out, loff_t* off_out, size_t len, unsigned int flags);
 * fd_in是用于输入数据的文件描述符，如果fd_in是一个管道文件描述符，那么off_in必须被设置为NULL。
 * 如果fd_in不是一个管道文件描述符(如socket)，那么off_in表示从输入数据流的何处开始读取数据。fd_out/off_out参数含义通fd_in/off_in，不过用于输出数据流
 * len参数制定移动数据的长度
 * flag参数控制数据如何移动
 * 
 * 零拷贝函数还有：
 *      管道之间拷贝：ssize_t tee(int fd_in, int fd_out, size_t len, unsigned int flags);
 * 
 */
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[])
{

    if (argc <= 2)
    {
        printf("Usage:%d, ip,port\n", argc);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock > 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int ret = bind(sock, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t cli_len = sizeof(client);
    int connfd = accept(sock, (struct sockaddr *)&client, &cli_len);
    if (connfd < 0)
    {
        printf("errno:%d", errno);
    }
    else
    {
        int pipefd[2];
        ret = pipe(pipefd); //创建管道

        /*
        实现简单高效的回射服务，整个过程未执行recv/send操作，因此也未涉及用户空间和内核空间之间的拷贝
        */
        //将connfd上流入的客户端数据定向到管道中
        ret = splice(connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret != -1);

        //将管道的输出定向到connfd客户端连接文件描述符
        ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret != -1);
        close(connfd);
    }
    close(sock);
    return 0;
}