
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
    static Log *GetInstance();  // C++11以后,使用局部变量懒汉不用加锁
    static void *FlushLogThread(
        void *args);  //异步写日志公有方法，调用私有方法async_write_log

    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列、日志等级、日志开关
    bool Init(const char *file_name, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0, int level = 1);
    void WriteLog(int level, const char *format,
                  ...);  //将输出内容按照标准格式整理

    void Flush(void);  //强制刷新缓冲区
    inline int getLevel() { return level_; }
    inline bool getSwitch() { return log_switch_; }

private:
    Log();
    virtual ~Log();
    void *AsyncWriteLog();

private:
    char dir_name_[128];  //路径名
    char log_name_[128];  // log文件名
    int split_lines_;     //日志最大行数
    int log_buf_size_;    //日志缓冲区大小
    long long count_;     //日志行数记录
    int today_;           //因为按天分类,记录当前时间是那一天
    FILE *file_pointer_;  //打开log的文件指针
    char *buf_;
    BlockQueue<std::string> *log_queue_;  //阻塞队列
    bool is_async_;                       //是否同步标志位
    Locker mutex_;
    int level_;
    bool log_switch_;  // 开关，构造函数中初始化为false， init初始化为true.
                       // true为开启日志， false为关闭日志
};

#define LOG_BASE(level, format, ...)                          \
    do {                                                      \
        Log *log = Log::GetInstance();                        \
        if (log->getSwitch() && (level >= log->getLevel())) { \
            log->WriteLog(level, format, ##__VA_ARGS__);      \
            log->Flush();                                     \
        }                                                     \
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
