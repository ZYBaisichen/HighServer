/*** 
 * @Author: baisichen
 * @Date: 2023-01-31 17:23:57
 * @LastEditTime: 2023-02-02 19:29:10
 * @LastEditors: baisichen
 * @Description: 升序定时器链表；升序定时器链表将其中的定时器按照超时时间做升序排序
 */
// #ifdef LST_TIMER
// #define LST_TIMER
#include <timer.h>
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

#define BUFFER_SIZE 64
class util_timer; //前向声明

//用户数据结构：客户端socket地址、socket文件描述符、读缓存和定时器. 很类似于query context
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer * timer;
};

//定时器类
class util_timer
{
public:
    util_timer():

public:
    time_t expire;//任务的超时时间，这里使用绝对时间
    void (*cb_func) (client_data*); //任务回调函数
    client_data* user_data;//回掉函数处理的客户数据，由定时器的执行者传递给回调函数
    util_timer* prev; //指向前一个定时器
    util_timer* next; //指向下一个定时器
};

//定时器链表。它是一个升序、双向链表，且带有头结点和尾节点
class sort_timer_lst {
public:
    sort_timer_lst(): head(NULL), tail(NULL){}
    //链表被销毁时，删除其中所有的定时器
    ~sort_timer_lst() {
        util_timer* tmp = head;
        while(tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    //将目标定时器timer添加到链表中
    void add_timer(util_timer* timer) {
        if (!timer) {
            return;
        }
        if (!head) {
            head = tail = timer;
            return;
        }
        //如果目标定时器的超时时间小于当前链表中所有定时器的超时时间，则把该定时器插入链表头部作为链表的头节点，否则就需要调用重载函数add_timer(util_timer* timer, utile_timer* lst_head), 把它插入链表中合适的位置，以保证链表的升序特性
        if (timer->expire < head->expire) {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    //当某个定时任务发生变化时，调整对应的定时器再链表中的位置。这个函数只考虑被调整的定时器的超时时间延长的情况，即该定时器需要往链表的尾部移动
    void adjust_timer(util_timer* timer) {
        if (!timer) {
            return;
        }
        util_timer* tmp = timer->next;
        //如果被调整的目标定时器处在链表尾部，或者定时器新的超时值仍然小于下一个定时器的超时值，则不用调整
        if (!tmp || (timer->expire < tmp->expire)) {
            return;
        }
        //如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表
        if (timer == head) {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        } else {//如果目标定时器不是链表的头结点，则将该定时器从关联表中取出，然后插入其原来所在位置之后的部分链表中
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }
    //将目标定时器timer从关联表中删除
    //O(1)删除
    void del_timer(util_timer* timer) {
        if (!timer) {
            return;
        }
        //当链表中只有一个定时器，即目标定时器
        if (timer == head && timer == tail) {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        //如果链表中至少有两个定时器，且目标定时器是链表的头节点，则讲链表的头节点重置为原头结点的下一个节点，然后删除目标定时器
        if (timer == head) {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        //如果链表中至少有两个定时器，且目标定时器是链表的尾节点，则讲链表的尾节点重置为原尾部节点的前一个节点，然后剔除目标定时器
        if (timer == tail) {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        //目标定时器位于链表中间，则讲它前后的定时器串联起来，然后删除目标定时器
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    //SIGALRM信号每次被触发就在其信号处理函数(如果使用统一事件源，则是主函数)中执行一次tick函数，以处理链表上到期的任务
    //执行定时任务的时间复杂度为O(1)
    void tick() {
        if (!head) {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(NULL);//获取系统当前时间
        util_timer* tmp = head;
        //从头结点开始依次处理每个定时器，直到遇到一个尚未到期的定时器，这就是定时器的核心逻辑
        while(tmp) {
            //因为每个定时器都使用绝对时间作为超时值，所以我们可以把定时器的超时值和系统当前时间，比较以判断定时器是否到期
            if (cur < tmp->expire) {
                break;
            }
            //调用定时器的回调函数，以执行定时任务
            tmp->cb_func(tmp->user_data);
            //执行完定时器中的定时任务后，就将它从链表中删除，并重置链表头节点
            head = tmp->next;
            if (head) {
                head->prev=NULL;
            }
            delete tmp;
            tmp = head;
        }
    }
private:
    //一个重载的辅助函数，它被公有的add_timer函数和adjust_timer函数调用。该函数表示将目标定时器timer添加到节点lst_head之后的部分链表中
    //添加定时器的时间复杂度为O(n)
    void add_timer(util_timer* timer, util_timer* lst_head) {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        //遍历lst_head节点之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间的节点，并将目标定时器插入该节点之前
        while(tmp) {
            if (timer->expire < tmp->expire) {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        } 
        //找不到比timer大的，插到末尾
        if (!tmp) {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }
private:
    util_timer* head;
    util_timer* tail;
};

// #endif