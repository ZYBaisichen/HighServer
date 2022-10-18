/*** 
 * @Author: baisichen
 * @Date: 2022-10-18 16:31:59
 * @LastEditTime: 2022-10-18 16:40:33
 * @LastEditors: baisichen
 * @Description: 第一个socket服务器
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
using  namespace  std;
static bool stop = false;
/*
超过5个链接时，第6个链接将处于SYN_SENT状态，这就是最大监听队列backlog的作用
tcp4       0      0  172.24.180.249.50358   172.24.180.249.12345   SYN_SENT
tcp4       0      0  172.24.180.249.12345   172.24.180.249.50130   ESTABLISHED
tcp4       0      0  172.24.180.249.50130   172.24.180.249.12345   ESTABLISHED
tcp4       0      0  172.24.180.249.12345   172.24.180.249.50121   ESTABLISHED
tcp4       0      0  172.24.180.249.50121   172.24.180.249.12345   ESTABLISHED
tcp4       0      0  172.24.180.249.12345   172.24.180.249.50103   ESTABLISHED
tcp4       0      0  172.24.180.249.50103   172.24.180.249.12345   ESTABLISHED
tcp4       0      0  172.24.180.249.12345   172.24.180.249.50094   ESTABLISHED
tcp4       0      0  172.24.180.249.50094   172.24.180.249.12345   ESTABLISHED
tcp4       0      0  172.24.180.249.12345   172.24.180.249.61953   ESTABLISHED
tcp4       0      0  172.24.180.249.61953   172.24.180.249.12345   ESTABLISHED
*/
//sigterm信号处理函数，触发时结束主程序中的循环
static void handle_term(int sig) {
	stop = true;
}
int main(int argc, char * argv[])
{
	signal(SIGTERM, handle_term);
	if (argc <= 3) {
		printf("usage %s ip_address port number backlog\n", argv[0]);
		return 1;
	}
	const char *ip = argv[1];
	int port = atoi(argv[2]);
	int backlog = atoi(argv[3]);

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	assert(sock >= 0);

	//创建一个IPv4 socket地址
	struct sockaddr_in address;
	bzero(&address, sizeof (address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr); //主机序转网络字节序ip
	address.sin_port = htons(port); //转网络字节序short到short

	int ret = bind(sock, (struct  sockaddr*)&address, sizeof (address)); //将socket绑定监听一个socket地址
	assert(ret != -1);

	ret = listen(sock, backlog); //监听端口
	assert(ret != -1);

	//循环等待链接，直到有SIGTERM信号将它中断
	while (!stop) {
		sleep(1);
	}

	//关闭socket
	close(sock);
	return 0;
}