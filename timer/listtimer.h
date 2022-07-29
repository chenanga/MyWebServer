
#ifndef CHENWEB_TIMER_LISTTIMER_H
#define CHENWEB_TIMER_LISTTIMER_H

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <csignal>
#include <cstring>
#include <iostream>

#include "../http/http_conn.h"
#include "../log/log.h"

// 定时器类
class Timer;

// 用户数据结构
struct ClientData {
    sockaddr_in address;  //客户端socket地址
    int sock_fd;          // socket文件描述符
    Timer *timer;         //定时器
};

class Timer {
public:
    Timer() : prev_(), next_() {}

    void (*CallbackFunction)(ClientData *);  //回调函数
    ClientData *user_data_;                  //连接资源
    Timer *prev_;                            //前向定时器
    Timer *next_;                            //后继定时器
    time_t expire_;                          //超时时间
};

class ListTimer {
public:
    ListTimer();
    ~ListTimer();  // 常规销毁链表，删除所有定时器

    void AddTimer(Timer *timer);  // 将目标定时器timer添加到链表中
    void DelTimer(Timer *timer);  // 删除目标定时器timer
    // 调整定时器，任务发生变化时，调整定时器在链表中的位置
    void AdjustTimer(Timer *timer);
    void Tick();

private:
    Timer *head_;  // 头节点
    Timer *tail_;  // 尾节点

    void AddTimerPrivateWay(Timer *timer);
};

class TimerUtils {
public:
    TimerUtils() = default;
    ~TimerUtils() = default;

    void Init(int time_slot);
    int SetNonBlocking(int fd);  // 设置非阻塞
    void AddFd(int epoll_fd, int fd, bool one_shot, int trigger_mode);
    static void SignalHandler(int sig);  // 信号处理函数
    void AddSignal(int sig, void(handler)(int), bool restart = true);
    void TimerHandler();  // 定时处理任务

public:
    static int *pipe_fd_;
    ListTimer timer_list_;
    static int epoll_fd_;
    int time_slot_;
};

void CallbackFunction(ClientData *user_data);

#endif  // CHENWEB_TIMER_LISTTIMER_H
