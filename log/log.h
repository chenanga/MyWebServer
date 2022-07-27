
#ifndef MYWEB_LOG_H
#define MYWEB_LOG_H

#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "block_queue.h"

class Log {
public:
    // C++11以后,使用局部变量懒汉不用加锁
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    //异步写日志公有方法，调用私有方法async_write_log
    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
    }
    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列、日志等级、日志开关
    bool init(const char *file_name, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0, int level = 1);
    void write_log(int level, const char *format,
                   ...);  //将输出内容按照标准格式整理

    void flush(void);  //强制刷新缓冲区
    inline int getLevel() { return m_level; }
    inline bool getSwitch() { return m_log_switch; }

private:
    Log();
    virtual ~Log();
    void *async_write_log() {
        std::string single_log;
        //从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(single_log)) {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];  //路径名
    char log_name[128];  // log文件名
    int m_split_lines;   //日志最大行数
    int m_log_buf_size;  //日志缓冲区大小
    long long m_count;   //日志行数记录
    int m_today;         //因为按天分类,记录当前时间是那一天
    FILE *m_fp;          //打开log的文件指针
    char *m_buf;
    BlockQueue<std::string> *m_log_queue;  //阻塞队列
    bool m_is_async;                       //是否同步标志位
    Locker m_mutex;
    int m_level;  // 开关，构造函数中初始化为false， init初始化为true
    bool m_log_switch;
};

#define LOG_BASE(level, format, ...)                      \
    do {                                                  \
        Log *log = Log::get_instance();                   \
        if (log->getSwitch() && log->getLevel()) {        \
            log->write_log(level, format, ##__VA_ARGS__); \
            log->flush();                                 \
        }                                                 \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do {                                   \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_INFO(format, ...)              \
    do {                                   \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_WARN(format, ...)              \
    do {                                   \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_ERROR(format, ...)             \
    do {                                   \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif  // MYWEB_LOG_H
