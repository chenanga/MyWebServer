

#ifndef CHENWEB_LISTTIMER_H
#define CHENWEB_LISTTIMER_H

#include <iostream>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <cassert>

#include "../http/http_conn.h"
#include "../log/log.h"

// 前向声明
class Timer;

// 用户数据结构
struct client_data {

    sockaddr_in address;  //客户端socket地址
    int sock_fd;  //socket文件描述符
    Timer* timer;  //定时器
};



class Timer {

public:
    Timer() : prev(), next(){ }

    time_t expire;  //超时时间
    void (*cb_func)( client_data* );  //回调函数
    client_data* user_data;  //连接资源
    Timer* prev;  //前向定时器
    Timer* next;  //后继定时器

};

class ListTimer {

public:
    ListTimer();
    ~ListTimer();  // 常规销毁链表，删除所有定时器

    // 将目标定时器timer添加到链表中
    void add_timer(Timer *timer);

    // 调整定时器，任务发生变化时，调整定时器在链表中的位置
    void adjust_timer(Timer *timer);

    // 删除目标定时器timer
    void del_timer(Timer *timer);

    void tick();

private:
    Timer *head;  // 头节点
    Timer *tail;  // 尾节点

    void add_timer(Timer *timer, Timer* last_head);
};

class TimerUtils
{
public:
    TimerUtils() {}
    ~TimerUtils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    ListTimer m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);


#endif //CHENWEB_LISTTIMER_H
