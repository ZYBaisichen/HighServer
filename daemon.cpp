/*** 
 * @Author: baisichen
 * @Date: 2022-10-23 22:51:26
 * @LastEditTime: 2022-10-23 23:06:29
 * @LastEditors: baisichen
 * @Description: 实现将服务器程序以守护进程的方式运行，功能同daemon系统调用
 *  int daemon(int nochdir, int noclose);
 *   nochdir制定是否改变工作目录，值为0时工作目录将被设置为"/"，否则继续使用当前目录。noclose参数为0时，标准输入、标准输出和标准错误输出都被重定向到/dev/null文件，否则依然使用原来的设备
 * 
 * 守护进程(Daemon Process)，也就是通常说的 Daemon 进程，是Linux中的后台服务进程，它是一个生存期较长的进程，通常独立于控制终端并且周期性的执行某种任务或等待处理某些发生的事件，一般采用d结尾的名字：
 * 参考知乎：https://zhuanlan.zhihu.com/p/343563584
 */

#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <cstdlib> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
using namespace std;
bool daemonize() {
    //创建子进程，关闭父进程，这样可以使程序在后台运行
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    } else if (pid>0) {
        exit(0);
    }
    //设置文件权限掩码，当进程创建新文件(使用open(const char* pathname, int flags, mode_t mode)系统调用)时，文件的权限将mode & 0777
    //这里是为了防止继承的文件有超过本用户权限问题
    umask(0);

    //创建新的会话，设置本进程为进程组的首领
    pid_t sid = setsid();
    if (sid < 0) {
        return false;
    }

    //切换工作目录
    if ((chdir("/")) < 0) {
        return false;
    }

    //关闭标准输入设备、标准输出设备和标准错误输出设备
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //关闭其他已经打开的文件描述符

    //将标准输入、标准输出和标准错误输出都定向到/dev/null文件
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("dev/null", O_RDWR);
    return true;
}
int main() {
    return 0;
}