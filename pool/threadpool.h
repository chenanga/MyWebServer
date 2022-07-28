#ifndef CHENWEB_POLL_H
#define CHENWEB_POLL_H

#include <mysql/mysql.h>
#include <pthread.h>

#include <exception>
#include <list>

#include "../http/http_conn.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

// 定义为模板类， 为了复用， T是任务类
template <typename T>
class ThreadPool {
public:
    ThreadPool(SqlConnPool *conn_pool, int thread_number = 10,
               int max_requests = 20000);
    ~ThreadPool();
    bool Append(T *request);  // 添加任务对象

private:
    int thread_number_;          // 线程的数量
    int max_requests_;           // 请求队列中允许的最大请求数
    pthread_t *threads_;         // 线程池
    std::list<T *> work_queue_;  // 请求队列
    Locker queue_locker_;        // 保护请求队列的互斥锁
    Sem queue_stat_;             // 是否有任务需要处理
    bool stop_;                  // 是否结束线程
    SqlConnPool *conn_pool_;     //数据库

    static void *Worker(void *arg);
    void Run();  // 工作线程运行的函数，不断从工作队列取出任务并执行
};

#endif