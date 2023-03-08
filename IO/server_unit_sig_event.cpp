/*** 
 * @Author: baisichen
 * @Date: 2022-10-31 20:58:08
 * @LastEditTime: 2023-01-31 17:22:20
 * @LastEditors: baisichen
 * @Description: 统一事件源
 * 1. 把信号的主要处理逻辑放到程序的主循环中，当信号处理函数被触发时，它只是简单地通知主循环程序接收信号，并把信号值传递给主循环，主循环再根据接收到的信号值执行目标信号对应的逻辑代码。
 * 2. 信号处理函数通常使用管道来将信号“传递”给主循环：信号处理函数往管道的写端写入信号值，主循环则从管道的读端读出该信号值。主循环使用I/O复用系统调用来监听管道的读端文件描述符上的可读事件，就能知道管道上何时有数据可读。如此一来，信号事件就能和其他I/O事件一样被处理，即统一事件源。
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

//信号处理函数
void sig_handler(int sig) {
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性。
    //所谓可重入就是函数打断也能恢复原样，这里用到了全局变量errno所以要恢复下
    int save_errno = errno;
    int msg = sig;
    printf("get signale:%d\n", msg);
    send(pipefd[1], (char*)&msg, 1, 0);//将信号值写入管道，以通知主循环
    errno = save_errno;
}

//设置信号的处理函数
void addsig(int sig) {
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

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    //使用socketpair创建管道，注册pipefd[0]上的可读事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    //设置信号的处理函数
    addsig(SIGHUP); //控制终端挂起
    addsig(SIGCHLD); //子进程状态发生变化（退出或暂停）
    addsig(SIGTERM); //终止进程，kill命令默认发送的信号就是SIGTERM
    addsig(SIGINT); //键盘输入以中断进程
    int stop_server = false;

    while (!stop_server)
    {
        //number为就绪的文件描述符个数，就绪的事件放在了events中
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
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
            } else if ((sockfd == pipefd[0]) && (events[i].events&EPOLLIN)) {
                //pipefd上有可读事件
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    //因为每个信号值占1个字节，所以按字节来逐个接收信号，我们以SIGTERM和SIGINT为例，说明了怎么安全终止服务器主循环
                    for (int i=0;i<ret;i++) {
                        switch (signals[i]) {
                            case SIGCHLD:
                            case SIGHUP: 
                            {
                                continue;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
                
            } else {
                printf("something else happened\n");
            }
        }
    
    }
    printf("close fds\n");
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}