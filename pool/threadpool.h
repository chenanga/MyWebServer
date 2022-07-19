#ifndef THREAD_POLL_H
#define THREAD_POLL_H

#include <pthread.h>
#include <list>
#include <exception>

#include "../lock/locker.h"


// 定义为模板类， 为了复用， T是任务类
template<typename T>
class Thread_pool {

public:
    Thread_pool(int thread_number = 8, int max_requests = 20000);
    ~Thread_pool();
    bool append(T * request);

private:
    int thread_number_;  // 线程的数量
    int max_requests_;  // 请求队列中允许的最大请求数
    pthread_t * threads_;  // 线程池
    std::list<T *> work_queue_;  // 请求队列
    Locker queue_locker_;  // 保护请求队列的互斥锁
    Sem queue_stat_;  // 是否有任务需要处理
    bool stop_;  // 是否结束线程

    
    static void* worker(void * arg) ;  // 工作线程运行的函数，不断从工作队列取出任务并执行
    void run();  // 
};

#endif