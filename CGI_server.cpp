/*** 
 * @Author: baisichen
 * @Date: 2022-10-19 16:35:06
 * @LastEditTime: 2022-10-19 20:49:21
 * @LastEditors: baisichen
 * @Description: CGI服务器原理，就是服务器和本地程序之间的交互
 *     CGI 是Web 服务器运行时外部程序的规范,按CGI 编写的程序可以扩展服务器功能。 
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
using namespace std;
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage: %s ip_address port_numbser\n", argc);
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret!=-1);

    ret = listen(sock, 5);
    assert(ret !=-1);

    struct sockaddr_in client;
    socklen_t cli_len = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &cli_len);
    if (connfd < 0) {
        printf("errno is: %d\n", errno);
    } else {
        /*
            1. 先关闭标准输出文件描述符STDOUT_FILENO（其值为1）
            2. 使用dup创建了一个新的描述符，其值为1(因为dup总是返回返回系统中最小的可用文件描述符)，这个描述符和connfd指向的文件、管道和网络连接相同
            3. 这样一来使用printf语句输出到标准输出的内容(这里是“abcd”)就会直接发送到与客户连接对应的socket上。
            4. 因此printf调用的输出将被客户端获得(而不是显示在服务器程序的终端上)，这就是CGI服务器的基本工作原理
        */
        close(STDOUT_FILENO);
        dup(connfd); 
        printf("abcd\n");
        close(connfd);
    }
    close(sock);
    return 0;
    

}