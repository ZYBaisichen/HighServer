/*** 
 * @Author: baisichen
 * @Date: 2022-10-19 16:35:06
 * @LastEditTime: 2022-10-20 17:04:32
 * @LastEditors: baisichen
 * @Description: web服务器集中写
 */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <libgen.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>     //一些文件打开，读写操作
#include <sys/stat.h>  //获取文件状态相关的函数
#include <sys/types.h> //一些数据类型的重命名或者是定义
#include <sys/uio.h>   //包含一些高级IO函数，分散读和分散写
#include <libgen.h>
#include <assert.h>
#include <errno.h>

#define BUFFER_SIZE 1024
//定义两种HTTP状态码和状态信息
static const char *status_line[2] = {"200 OK", "500 Internal server error"};

using namespace std;
/*
./server 10.81.114.214 9030 test.txt
telnet 10.81.114.214 9030

下面4行即为服务器发送给client的文件信息
HTTP/1.1 200 OK
Content-Length: 8

abcdefg


IP 10.81.114.214.9030 > 10.81.112.77.29039: Flags [P.], seq 1:47, ack 1, win 114, options [nop,nop,TS val 3253060265 ecr 3253083112], length 46
	0x0000:  4500 0062 9a74 4000 4006 a85c 0a51 72d6
	0x0010:  0a51 704d 2346 716f d7b4 58dd 783c 07c1
	0x0020:  8018 0072 f819 0000 0101 080a c1e5 c2a9
	0x0030:  c1e6 1be8 4854 5450 2f31 2e31 2032 3030
	0x0040:  204f 4b0d 0a43 6f6e 7465 6e74 2d4c 656e
	0x0050:  6774 683a 2038 0d0a 0d0a 6162 6364 6566
	0x0060:  670a

*/
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("usage: %d ip_address port_numbser filename\n", argc);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *file_name = argv[3];

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int ret = bind(sock, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    int cli_len = sizeof(client);
    int connfd = accept(sock, (struct sockaddr *)&client, (socklen_t *)&cli_len);
    if (connfd < 0)
    {
        printf("errno is:%d\n", errno);
    }
    else
    {
        char header_buf[BUFFER_SIZE]; //TCP
        char *file_buf;               //目标文件内容的应用程序缓存
        struct stat file_stat;        //用于获取目标文件属性，如是否为目录，文件大小等
        bool valid = true;            //记录目标文件是否有效并读取成功
        int len = 0;                  //记录缓存区header_bug目前已经使用了多少字节的空间
        if (stat(file_name, &file_stat) < 0)
        { //目标文件不存在
            valid = false;
        }
        else
        {
            if (S_ISDIR(file_stat.st_mode))
            { //目标文件是目录
                valid = false;
            }
            else if (file_stat.st_mode & S_IROTH)
            {
                //动态分配缓存区file_buf，并指定其大小为目标文件的大小file_stat.st_size加1，然后将目标文件读入缓存区file_buf中
                int fd = open(file_name, O_RDONLY);
                file_buf = new char[(int)(file_stat.st_size + 1)];
                memset(file_buf, '\0', file_stat.st_size + 1);
                if (read(fd, file_buf, file_stat.st_size) < 0)
                {
                    valid = false;
                }
            }
            else
            {
                valid = false;
            }
        }
        if (valid)
        {
            //加入应答状态行、标准HTTP第1行
            ret = snprintf(header_buf, BUFFER_SIZE - 1, "%s %s\r\n", "HTTP/1.1", status_line[0]);
            len += ret;
            //加入conten-legth长度，标准HTTP第2行
            ret = snprintf(header_buf + len, BUFFER_SIZE - 1 - len, "Content-Length: %lld\r\n", file_stat.st_size);
            len += ret;
            //加入一个空行，标准HTTP第3行
            ret = snprintf(header_buf + len, BUFFER_SIZE - 1 - len, "%s", "\r\n");

            //利用writev将header_buf和file_buf的内容一并写入(集中写)
            struct iovec iv[2];
            iv[0].iov_base = header_buf;
            iv[0].iov_len = strlen(header_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(connfd, iv, 2);
        }
        else
        { //目标文件无效，则通知客户端服务器发上了内部错误
            ret = snprintf(header_buf, BUFFER_SIZE - 1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
            len += ret;
            ret = snprintf(header_buf, BUFFER_SIZE - 1 - len, "%s", "\r\n");
            send(connfd, header_buf, strlen(header_buf), 0);
        }
        close(connfd);
        delete[] file_buf;
    }
    close(sock);
    return 0;
}
