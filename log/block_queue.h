
#ifndef CHENWEB_BLOCK_QUEUE_H
#define CHENWEB_BLOCK_QUEUE_H

#include <pthread.h>
#include <sys/time.h>

#include <cstdlib>
#include <iostream>

#include "../lock/locker.h"

template <class T>
class BlockQueue {
public:
    BlockQueue(int max_size = 1000);
    ~BlockQueue();

    bool push(const T &item);  // 往队列添加元素
    bool pop(T &item);  // 从队列中取元素,如果当前队列没有元素,将会等待条件变量
    bool pop(T &item, int ms_timeout);  // 指定等待时间的pop
    bool full();                        // 判断队列是否满了
    bool empty();                       // 判断队列是否为空
    bool front(T &value);               //返回队首元素
    bool back(T &value);                //返回队尾元素
    int size();                         // 获取队列大小
    int max_size();                     // 获取队列最大容量
    void clear();                       // 清空队列

private:
    Locker mutex_;
    Cond cond_;
    T *array_;

    int size_;
    int max_size_;
    int front_;
    int back_;
};

template <typename T>
BlockQueue<T>::BlockQueue(int max_size) {
    if (max_size <= 0) exit(-1);

    max_size_ = max_size;
    array_ = new T[max_size];
    size_ = 0;
    front_ = -1;
    back_ = -1;
}

template <class T>
BlockQueue<T>::~BlockQueue() {
    mutex_.lock();
    if (array_ != nullptr) delete[] array_;
    mutex_.unlock();
}

template <class T>
void BlockQueue<T>::clear() {
    mutex_.lock();
    size_ = 0;
    front_ = -1;
    back_ = -1;
    mutex_.unlock();
}

template <class T>
bool BlockQueue<T>::full() {
    mutex_.lock();
    if (size_ >= max_size_) {
        mutex_.unlock();
        return true;
    }
    mutex_.unlock();
    return false;
}

template <class T>
bool BlockQueue<T>::empty() {
    mutex_.lock();
    if (0 == size_) {
        mutex_.unlock();
        return true;
    }
    mutex_.unlock();
    return false;
}

template <class T>
bool BlockQueue<T>::front(T &value) {
    mutex_.lock();
    if (size_ == 0) {
        mutex_.unlock();
        return false;
    }
    value = array_[front_];
    mutex_.unlock();
    return true;
}

template <class T>
bool BlockQueue<T>::back(T &value) {
    mutex_.lock();
    if (0 == size_) {
        mutex_.unlock();
        return false;
    }
    value = array_[back_];
    mutex_.unlock();
    return true;
}

template <class T>
int BlockQueue<T>::size() {
    int tmp = 0;
    mutex_.lock();
    tmp = size_;
    mutex_.unlock();
    return tmp;
}

template <class T>
int BlockQueue<T>::max_size() {
    int tmp = 0;
    mutex_.lock();
    tmp = max_size_;
    mutex_.unlock();
    return tmp;
}

template <class T>
bool BlockQueue<T>::push(const T &item) {
    mutex_.lock();
    if (size_ >= max_size_) {
        cond_.broadcast();
        mutex_.unlock();
        return false;
    }

    back_ = (back_ + 1) % max_size_;
    array_[back_] = item;
    size_++;
    cond_.broadcast();
    mutex_.unlock();
    return true;
}

template <class T>
bool BlockQueue<T>::pop(T &item) {
    mutex_.lock();
    while (size_ <= 0) {
        if (!cond_.wait(mutex_.get())) {
            mutex_.unlock();
            return false;
        }
    }

    front_ = (front_ + 1) % max_size_;
    item = array_[front_];
    size_--;
    mutex_.unlock();
    return true;
}

template <class T>
bool BlockQueue<T>::pop(T &item, int ms_timeout) {
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    mutex_.lock();
    if (size_ <= 0) {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!cond_.timewait(mutex_.get(), t)) {
            mutex_.unlock();
            return false;
        }
    }

    if (size_ <= 0) {
        mutex_.unlock();
        return false;
    }

    front_ = (front_ + 1) % max_size_;
    item = array_[front_];
    size_--;
    mutex_.unlock();
    return true;
}

/*
 * 往队列添加元素，需要将所有使用队列的线程先唤醒
 * 当有元素push进队列,相当于生产者生产了一个元素
 * 若当前没有线程等待条件变量,则唤醒无意义
 * */
#endif  // CHENWEB_BLOCK_QUEUE_H
