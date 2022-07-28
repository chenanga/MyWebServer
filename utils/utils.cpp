
#include "utils.h"

// 添加信号捕捉
void AddSig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

// 设置文件描述符非阻塞
void SetNoneBlocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

// 添加文件描述符到epoll中, mode: 0, LT模式  1, ET模式
void AddFd(int epoll_fd, int fd, bool one_shot, int mode) {
    epoll_event event;
    event.data.fd = fd;

    if (mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot) event.events |= EPOLLONESHOT;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    SetNoneBlocking(fd);  // 设置文件描述符非阻塞
}

//从内核时间表删除描述符
void RemoveFd(int epoll_fd, int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符, mode: 0, LT模式  1, ET模式
void ModifyFd(int epoll_fd, int fd, int ev, int mode) {
    epoll_event event;
    event.data.fd = fd;

    if (mode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}
