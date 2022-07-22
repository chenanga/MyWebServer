#include "threadpool.h"



template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests)
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
        std::cout << "success create the " << i << "th thread" << std::endl;
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {

    delete[] threads_;
    stop_ = true;
}

template<typename T>
void* ThreadPool<T>::worker(void * arg) {
    // 由于静态成员函数不能访问非静态成员函数，这里采用的办法是把this指针传递进来，用一个threadpool对象接受，间接访问
    ThreadPool* pool = (ThreadPool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void ThreadPool<T>::run() {
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

template <typename T>
bool ThreadPool<T>::append(T* request) {

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



// 显式实例化, 避免模板类实现和定义分开编译出错
template class ThreadPool<HttpConn>;


