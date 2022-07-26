#include "log.h"


Log::Log() {
    m_count = 0;
    m_is_async = false;
    m_level = false;
}

Log::~Log() {
    if (m_fp != nullptr) {
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size, int level) {
    //如果设置了max_queue_size,则设置为异步
    m_level = level;
    m_log_switch = true;

    if (max_queue_size >= 1) {
        m_is_async = true;  //设置写入方式flag
        m_log_queue = new BlockQueue<std::string>(max_queue_size);   //创建并设置阻塞队列长度
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }
    //输出内容的长度
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;  //日志的最大行数

    time_t t = time(nullptr);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');  //从后往前找到第一个/的位置
    char log_full_name[512] = {0};

    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == nullptr) {
        snprintf(log_full_name, sizeof(log_full_name), "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else {
        strcpy(log_name, p + 1);  //将/的位置向后移动一个位置，然后复制到logname中
        strncpy(dir_name, file_name, p - file_name + 1);  //p - file_name + 1是文件所在路径文件夹的长度
        //dirname相当于./
        snprintf(log_full_name, sizeof(log_full_name), "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == nullptr) {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
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
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;  //更新现有行数

    //日志不是今天或写入的日志行数是最大行的倍数, m_split_lines为最大行数
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {  //everyday log

        char new_log[512] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
        //格式化日志名中的时间部分
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)  //如果是时间不是今天,则创建今天的日志，更新m_today和m_count
        {
            snprintf(new_log, sizeof(new_log), "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else {   //超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines
            snprintf(new_log, sizeof(new_log), "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    va_list vaList;
    va_start(vaList, format);  //将传入的format参数赋值给valst，便于格式化输出

    std::string log_str;
    m_mutex.lock();

    // 写入的具体时间内容格式, 时间 + 内容
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, vaList);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    // 若m_is_async为true表示异步，默认为同步, 若异步,则将日志信息加入阻塞队列, 同步则加锁向文件中写
    if (m_is_async && !m_log_queue->full()) {
        m_log_queue->push(log_str);
    }
    else {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(vaList);
}

void Log::flush(void) {
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
