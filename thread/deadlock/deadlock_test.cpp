/***
 * @Author: baisichen
 * @Date: 2023-03-07 17:14:52
 * @LastEditTime: 2023-03-07 17:22:44
 * @LastEditors: baisichen
 * @Description: 多线程场景下fork子进程
 */
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
using namespace std;

pthread_mutex_t mutex;

void *another(void *arg)
{
    printf("in child thread lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

void prepare() {
    pthread_mutex_lock(&mutex);
}

void infork() {
    pthread_mutex_unlock(&mutex);
}

int main()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_t id;
    pthread_create(&id, NULL, another, NULL);
    // 父进程中的主线程暂停1s，以确保在执行fork操作之前，子线程已经开始运行并获得了互斥变量mutex
    sleep(1);
    // int pid = fork();
    int pid = pthread_atfork(prepare, infork, infork); //prepare在fork创建出子进程之前使用，锁住所有父进程的互斥锁；第一个infork在船检出紫禁城后，fork返回前在父进程中被执行。第二个infork是fork返回前在子进程中执行
    if (pid < 0)
    {
        pthread_join(id, NULL);
        pthread_mutex_destroy(&mutex);
        return 1;
    }
    else if (pid == 0)
    {
        printf("i am in the child, want to get the lock\n");
        // 子进程从父进程继承了互斥锁mutex的状态，该互斥锁处于锁住的状态，这是由父进程中的子线程执行pthread_mutex_lock引起的。因此，下面这句加锁操作会一直是阻塞，尽管从逻辑上来说它是不应该阻塞的
        pthread_mutex_lock(&mutex);
        printf("i can not run to here, oop...\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    }
    else
    {
        wait(NULL);
    }
    pthread_join(id, NULL);
    pthread_mutex_destroy(&mutex);
    return 0;
}