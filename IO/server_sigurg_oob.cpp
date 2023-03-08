/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2022-11-09 21:07:11
 * @LastEditors: baisichen
 * @Description: 使用SIGURG信号处理OOB带外数据
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
#include <signal.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
static int pipefd[2];

static int connfd;

//信号处理函数
void sig_urg(int sig) {
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性。
    //所谓可重入就是函数打断也能恢复原样，这里用到了全局变量errno所以要恢复下
    int save_errno = errno;
    int msg = sig;
    printf("get signale:%d\n", msg);
    char buffer[TCP_BUFFER_SIZE];
    int ret = recv(connfd, buffer, TCP_BUFFER_SIZE-1, MSG_OOB); //接收带外数据
    printf("got %d bytes of oob data: %s\n", ret, buffer);
    errno = save_errno;
}

//设置信号的处理函数
void addsig(int sig, void (* sig_handler) (int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
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

    struct sockaddr_in cli_address;
    socklen_t cli_len = sizeof(cli_address);
    connfd = accept(listenfd, (struct sockaddr *)&cli_address, &cli_len);
    if (connfd < 0) {
        printf("errno is :%d", errno);
    } else {
        addsig(SIGURG, sig_urg);
        //使用SIGURG信号之前，我们必须设置socket的宿主进程或进程组
        fcntl(connfd, F_SETOWN, getpid());

        char buffer[TCP_BUFFER_SIZE];
        while (1) {
            memset(buffer, '\0', sizeof(buffer));
            ret = recv(connfd, buffer, TCP_BUFFER_SIZE-1, 0);
            if (ret <= 0) {
                break;
            }
            printf("get %d bytes of normal data %s\n", ret, buffer);
            
        }
        close(connfd);
    }
    printf("close fds\n");
    close(listenfd);
    return 0;
}