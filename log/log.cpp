#include "log.h"

Log::Log() {
    count_ = 0;
    is_async_ = false;
    level_ = false;
}

Log::~Log() {
    if (file_pointer_ != nullptr) {
        fclose(file_pointer_);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::Init(const char *file_name, int log_buf_size, int split_lines,
               int max_queue_size, int level) {
    //如果设置了max_queue_size,则设置为异步
    level_ = level;
    log_switch_ = true;

    if (max_queue_size >= 1) {
        is_async_ = true;  //设置写入方式flag
        log_queue_ = new BlockQueue<std::string>(
            max_queue_size);  //创建并设置阻塞队列长度
        pthread_t tid;
        // FlushLogThread,这里表示创建线程异步写日志
        pthread_create(&tid, nullptr, FlushLogThread, nullptr);
    }
    //输出内容的长度
    log_buf_size_ = log_buf_size;
    buf_ = new char[log_buf_size_];
    memset(buf_, '\0', log_buf_size_);
    split_lines_ = split_lines;  //日志的最大行数

    time_t t = time(nullptr);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');  //从后往前找到第一个/的位置
    char log_full_name[512] = {0};

    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == nullptr)
        snprintf(log_full_name, sizeof(log_full_name), "%d_%02d_%02d_%s",
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 file_name);
    else {
        strcpy(log_name_,
               p + 1);  //将/的位置向后移动一个位置，然后复制到logname中
        strncpy(
            dir_name_, file_name,
            p - file_name + 1);  // p - file_name + 1是文件所在路径文件夹的长度
        // dirname相当于./
        snprintf(log_full_name, sizeof(log_full_name), "%s%d_%02d_%02d_%s",
                 dir_name_, my_tm.tm_year + 1900, my_tm.tm_mon + 1,
                 my_tm.tm_mday, log_name_);
    }
    today_ = my_tm.tm_mday;
    file_pointer_ = fopen(log_full_name, "a");
    if (file_pointer_ == nullptr) return false;
    return true;
}

void Log::WriteLog(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    //日志分级
    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    mutex_.lock();
    count_++;  //更新现有行数

    //日志不是今天或写入的日志行数是最大行的倍数, split_lines_为最大行数
    if (today_ != my_tm.tm_mday ||
        count_ % split_lines_ == 0) {  // everyday log

        char new_log[512] = {0};
        fflush(file_pointer_);
        fclose(file_pointer_);
        char tail[16] = {0};
        //格式化日志名中的时间部分
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday);

        // 如果是时间不是今天,则创建今天的日志，更新m_today和m_count
        if (today_ != my_tm.tm_mday) {
            snprintf(new_log, sizeof(new_log), "%s%s%s", dir_name_, tail,
                     log_name_);
            today_ = my_tm.tm_mday;
            count_ = 0;
        } else  // 超过了最大行，在之前的日志名基础上加后缀, count_/split_lines_
            snprintf(new_log, sizeof(new_log), "%s%s%s.%lld", dir_name_, tail,
                     log_name_, count_ / split_lines_);

        file_pointer_ = fopen(new_log, "a");
    }
    mutex_.unlock();

    va_list vaList;
    va_start(vaList, format);  //将传入的format参数赋值给vaList，便于格式化输出
    std::string log_str;
    mutex_.lock();

    // 写入的具体时间内容格式, 时间 + 内容
    int n = snprintf(buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
    int m = vsnprintf(buf_ + n, log_buf_size_ - 1, format, vaList);
    buf_[n + m] = '\n';
    buf_[n + m + 1] = '\0';
    log_str = buf_;

    mutex_.unlock();

    // 若m_is_async为true表示异步，默认为同步, 若异步,则将日志信息加入阻塞队列,
    // 同步则加锁向文件中写
    if (is_async_ && !log_queue_->full())
        log_queue_->push(log_str);
    else {
        mutex_.lock();
        fputs(log_str.c_str(), file_pointer_);
        mutex_.unlock();
    }

    va_end(vaList);
}

void Log::Flush(void) {
    mutex_.lock();
    //强制刷新写入流缓冲区
    fflush(file_pointer_);
    mutex_.unlock();
}

void *Log::FlushLogThread(void *args) { Log::GetInstance()->AsyncWriteLog(); }

Log *Log::GetInstance() {
    static Log instance;
    return &instance;
}

void *Log::AsyncWriteLog() {
    std::string single_log;
    //从阻塞队列中取出一个日志string，写入文件
    while (log_queue_->pop(single_log)) {
        mutex_.lock();
        fputs(single_log.c_str(), file_pointer_);
        mutex_.unlock();
    }
}
