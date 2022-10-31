/*** 
 * @Author: baisichen
 * @Date: 2022-10-18 19:14:43
 * @LastEditTime: 2022-10-18 19:53:48
 * @LastEditors: baisichen
 * @Description: 
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <assert.h>

int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage: %s ip_address, port number\n", basename(argv[0]));
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    printf("111");
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    printf("222");
    assert(sockfd >= 0);
    printf("333");
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        printf("connection failed\n");
    }
    else
    {
        const char *oob_data = "abc";
        const char *normal_data = "123";
        /*
        服务端输出
        got 5 bytes of normal data '123ab'
        got 1 bytes of oob data 'c'
        got 3 bytes of normal data '123'
        */
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB); //带外信息
        send(sockfd, normal_data, strlen(normal_data), 0);
    }
    close(sockfd);

    return 0;
}