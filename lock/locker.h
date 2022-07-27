#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <semaphore.h>

#include <exception>

/*
    线程同步机制封装类
    类中主要是Linux下三种锁进行封装，将锁的创建于销毁函数封装在类的构造与析构函数中，实现RAII机制

*/
// 互斥锁类,
// 可以保护关键代码段,以确保独占式访问.当进入关键代码段,获得互斥锁将其加锁;离开关键代码段,唤醒等待该互斥锁的线程.
class Locker {
public:
    Locker() {
        if (pthread_mutex_init(&mutex_, NULL) != 0) throw std::exception();
    }

    ~Locker() {
        pthread_mutex_destroy(&mutex_);
        /*  //
           析构函数中不要抛出异常，因为栈展开容易导致资源泄露和程序崩溃，所以别让异常逃离析构函数。
                if (pthread_mutex_destroy(&mutex_) != 0)
                throw std::exception();
        */
    }

    // 上锁
    bool lock() { return pthread_mutex_lock(&mutex_) == 0; }

    // 解锁
    bool unlock() { return pthread_mutex_unlock(&mutex_) == 0; }

    // 获取
    pthread_mutex_t* get() { return &mutex_; }

private:
    pthread_mutex_t mutex_;
};

// 条件变量类,
// 条件变量提供了一种线程间的通知机制,当某个共享数据达到某个值时,唤醒等待这个共享数据的线程.
class Cond {
public:
    Cond() {
        if (pthread_cond_init(&cond_, NULL) != 0) throw std::exception();
    }

    ~Cond() { pthread_cond_destroy(&cond_); }

    // 等待
    bool wait(pthread_mutex_t* mutex) {
        return pthread_cond_wait(&cond_, mutex) == 0;
    }

    // 等待指定时间
    bool timewait(pthread_mutex_t* mutex, struct timespec t) {
        return pthread_cond_timedwait(&cond_, mutex, &t) == 0;
    }

    // 唤醒一个等待的线程
    bool signal() { return pthread_cond_signal(&cond_) == 0; }

    // 唤醒全部等待的线程
    bool broadcast() { return pthread_cond_broadcast(&cond_) == 0; }

private:
    pthread_cond_t cond_;
};

// 信号量类
class Sem {
public:
    Sem() {
        // - sem : 信号量变量的地址
        // - pshared : 0 用在线程间 ，非0 用在进程间
        // - value : 信号量中的值
        if (sem_init(&sem_, 0, 0) != 0) throw std::exception();
    }

    Sem(int num) {
        if (sem_init(&sem_, 0, num) != 0) throw std::exception();
    }

    ~Sem() { sem_destroy(&sem_); }

    // 等待信号量，调用一次对信号量的值-1，如果值为0，就阻塞
    bool wait() { return sem_wait(&sem_) == 0; }

    // 增加信号量，调用一次对信号量的值+1
    bool post() { return sem_post(&sem_) == 0; }

private:
    sem_t sem_;
};

#endif