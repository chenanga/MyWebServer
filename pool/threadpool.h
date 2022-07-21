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

template <typename T>
Thread_pool<T>::~Thread_pool() {

    delete[] threads_;
    stop_ = true;
}

template <typename T>
Thread_pool<T>::Thread_pool(int thread_number, int max_requests)
        : thread_number_(thread_number), max_requests_(max_requests), stop_(false), threads_(NULL) {
    if ((thread_number <= 0 || max_requests <= 0)) {
        // todo, log输出
        throw std::exception();
    }

    threads_ = new pthread_t[thread_number_];
    if (!threads_) {
        // todo, log输出
        throw std::exception();
    }

    // 创建线程，并设置为线程脱离
    for (int i = 0; i < thread_number_; ++ i) {
        if (pthread_create(threads_ + i, NULL, worker, this) != 0) {  // 函数原型中的第三个参数，为函数指针，指向处理线程函数的地址。该函数，要求为静态函数。如果处理线程函数为类成员函数时，需要将其设置为静态成员函数。
            delete[] threads_;
            // todo, log输出
            throw std::exception();
        }

        if (pthread_detach(threads_[i]) != 0) {
            delete[] threads_;
            // todo, log输出
            throw std::exception();
        }
    }

}



template <typename T>
bool Thread_pool<T>::append(T* request) {

    queue_locker_.lock();
    if (work_queue_.size() > max_requests_) {  // 超出最大允许数量
        queue_locker_.unlock();
        return false;
    }

    work_queue_.push_back(request);
    queue_stat_.post();  // 增加信号量
    queue_locker_.unlock();
    return true;

}


template<typename T>
void* Thread_pool<T>::worker(void * arg) {
    // 由于静态成员函数不能访问非静态成员函数，这里采用的办法是把this指针传递进来，用一个threadpool对象接受，间接访问
    Thread_pool* pool = (Thread_pool*)arg;
    pool->run();
    return pool;
}


template<typename T>
void Thread_pool<T>::run() {
    while (!stop_) {
        queue_stat_.wait();  // 如果信号量有值，不阻塞，继续执行
        queue_locker_.lock();
        if (work_queue_.empty()) {
            queue_locker_.unlock();
            continue;
        }

        T* request = work_queue_.front();
        work_queue_.pop_front();
        queue_locker_.unlock();

        if (!request)
            continue;

        request->process();  // 任务类的process函数

    }

}
#endif