/*** 
 * @Author: baisichen
 * @Date: 2023-02-26 23:02:11
 * @LastEditTime: 2023-03-08 15:41:54
 * @LastEditors: baisichen
 * @Description: 线程池类
 */
// #ifdef THREADPOOL_H
// #define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

//线程池累，将它定义为模板是为了代码复用，模板参数T是任务类
template <typename T>
class threadpool {
public:
    threadpool(int thread_number=8, int max_requests=10000);
    ~threadpool();
    //往请求队列中添加任务
    bool append(T* request);

private:
    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void* worker(void* arg);
    void run(int threadid=0);

private:
    int m_thread_number;//线程池中的线程数
    int m_max_requests; //请求队列中允许的最大请求数
    pthread_t* m_threads;//描述线程池的数组，大小为m_thread_number;
    std::list<T*> m_workqueue;//请求队列
    locker m_queuelocker;//保护请求队列的互斥锁
    /*是否有任务需要处理。这里之所以要使用信号量，是为了减少无意义的锁竞争
    如果使用信号量，每次信号量加一会通知其他线程处理，竞争到锁之后一定会有请求可以处理，而不是在竞争锁之后再判断有没有请求，如果没请求的话，这次锁竞争就无意义了*/
    sem m_queuestat;
    bool m_stop;//是否结束线程
    struct Param
    {
        void* obj;
        int threadid;
    };
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests): m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
    if ((thread_number<0) || (max_requests <= 0)) {
        throw std::exception();
    }
    printf("thread_num:%d\n", thread_number);

    m_threads = new pthread_t(m_thread_number);
    if (!m_threads) {
        throw std::exception();
    }
    
    //创建thread_number个线程，并将他们都设置成脱离线程
    for (int i=0; i<thread_number; i++) {
        printf("create the %dth thread\n", i);
        Param thread_param;
        thread_param.obj = (void*) this;
        thread_param.threadid = i;
        // if (pthread_create(m_threads+i , NULL, worker, &thread_param) !=0) {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}


template <typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request) {
    //操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    // Param td_param = *(Param*) arg;
    // threadpool *pool = (threadpool*) td_param.obj;
    // printf("cur threadid:%d\n", td_param.threadid);
    threadpool *pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(int threadid) {
    while(!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        // printf("cur threadid:%d, process request\n", threadid);
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}

// #endif