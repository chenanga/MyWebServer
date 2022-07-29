
#ifndef CHENWEB_WEBSERVER_H
#define CHENWEB_WEBSERVER_H

#include <arpa/inet.h>
#include <error.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../config/config.h"
#include "../global/global.h"
#include "../http/http_conn.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/listtimer.h"
#include "../utils/utils.h"

class WebServer {
public:
    WebServer(const std::string &config_file_name);
    ~WebServer();

    void Start();

private:
    int port_;

    int epoll_fd_;
    int listen_fd_;
    int trigger_mode_;
    int listen_fd_trigger_mode_;
    int client_fd_trigger_mode_;
    int elegant_close_linger_;  // 优雅关闭链接
    Config config_;
    int actor_model_;                    // 并发模型选择
    ThreadPool<HttpConn> *thread_pool_;  //线程池
    HttpConn *users_;                    // 客户端信息
    epoll_event events_[MAX_EVENT_NUMBER];

    // 日志相关
    int log_write_way_;          // 日志写入方式
    int log_is_open_;            // 日志开关
    int log_level_;              // 日志等级
    std::string log_file_path_;  // 日志文件路径

    // 定时器相关
    int pipe_fd_[2];
    ClientData *users_timer_;
    TimerUtils timer_utils_;
    static ListTimer timer_list_;

    // 数据库相关
    int sql_port_;  // 数据库端口
    std::string sql_user_;
    std::string sql_password_;
    std::string sql_database_name_;
    int sql_thread_num_;          // 数据库连接池线程数量
    SqlConnPool *sql_conn_pool_;  // 数据库连接池
    std::string sql_host_;        // 数据库地址

    void Init();             // config变量赋值
    void CheckPort() const;  //检查端口
    void SqlInit();          // 数据库初始化
    void LogInit();          // 日志初始化
    void SetTriggerMode();   //设置触发模式
    void EventListen();      // 监听

    bool DealClientConn();
    bool DealSignal(bool &timeout, bool &stop_server);
    void DealReadEvent(int sock_fd);
    void DealWriteEvent(int sock_fd);

    void SendError(int fd, const char *info);
    void SetTimer(int conn_fd, struct sockaddr_in client_address);
    void CloseTimer(Timer *timer, int sock_fd);
    void AdjustTimer(Timer *timer);
};

#endif  // CHENWEB_WEBSERVER_H
