

#include "listtimer.h"


ListTimer::ListTimer() : head(nullptr), tail(nullptr) {

}

ListTimer::~ListTimer() {
    Timer* node = head;
    while (node) {
        head = node->next;
        delete node;
        node = head;
    }
}

void ListTimer::add_timer(Timer *timer) {
    if(timer == nullptr)
        return;

    if(head == nullptr) {
        head = tail = timer;
        return;
    }

    //如果新的定时器超时时间小于当前头部结点
    //直接将当前定时器结点作为头部结点
    if(timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    //否则调用私有成员，调整内部结点
    add_timer( timer, head );
}

void ListTimer::adjust_timer(Timer *timer) {
    if(timer == nullptr)
        return;

    Timer* tmp = timer->next;

    //1. 被调整的定时器在链表尾部
    //2. 定时器超时值仍然小于下一个定时器超时值，不调整
    if(tmp == nullptr || ( timer->expire < tmp->expire ) )
        return;

    //被调整定时器是链表头结点，将定时器取出，重新插入
    if(timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }

    //被调整定时器在内部，将定时器取出，重新插入
    else {  // timer != head
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer( timer, timer->next );
    }
}

void ListTimer::del_timer(Timer *timer) {
    if (timer == nullptr)
        return;

    //链表中只有一个定时器，需要删除该定时器
    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }

    //被删除的定时器为头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }

    //被删除的定时器为尾结点
    if (timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }

    //被删除的定时器在链表内部，常规链表结点删除
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void ListTimer::add_timer(Timer *timer, Timer *last_head) {
    Timer* prev = last_head;
    Timer* tmp = prev->next;

    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while(tmp) {
        if(timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;  // 可以直接 return
        }
        prev = tmp;
        tmp = tmp->next;
    }

    //遍历完未找到位置，目标定时器需要放到尾结点处
    if(tmp == nullptr) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}

// SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务。
void ListTimer::tick() {
    if(head == nullptr)
        return;

    time_t cur = time(nullptr);  // 获取当前系统时间
    Timer* tmp = head;
    // 从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
    while(tmp) {
        /* 因为每个定时器都使用绝对时间作为超时值，所以可以把定时器的超时值和系统当前时间，
        比较以判断定时器是否到期*/
        if(cur < tmp->expire)
            break;

        // 调用定时器的回调函数，以执行定时任务
        tmp->cb_func(tmp->user_data);
        LOG_INFO("Client disconnect : conn_fd = %d, client_address = %s", tmp->user_data->sock_fd, inet_ntoa(tmp->user_data->address.sin_addr));

        // 执行完定时器中的定时任务之后，就将它从链表中删除，并重置链表头节点
        head = tmp->next;
        if(head)
            head->prev = nullptr;

        delete tmp;
        tmp = head;
    }
}


void TimerUtils::init(int timeslot) {
    m_TIMESLOT = timeslot;
    return;
}

//对文件描述符设置非阻塞
int TimerUtils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void TimerUtils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void TimerUtils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void TimerUtils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void TimerUtils::timer_handler()
{
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

void TimerUtils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *TimerUtils::u_pipefd = 0;
int TimerUtils::u_epollfd = 0;

class TimerUtils;
void cb_func(client_data *user_data)
{
    epoll_ctl(TimerUtils::u_epollfd, EPOLL_CTL_DEL, user_data->sock_fd, 0);
    assert(user_data);
    close(user_data->sock_fd);
    HttpConn::m_user_count--;
}