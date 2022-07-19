
#include "utils.h"

// 添加信号捕捉
void addSig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);

}

// 添加文件描述符到epoll中, mode: 0, LT模式  1, ET模式
void addFd(int epoll_fd, int fd, bool one_shot, int mode) {
    epoll_event event;
    event.data.fd = fd;

    if (mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
//    setnonblocking(fd);
}

//从内核时间表删除描述符
void removeFd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符, mode: 0, LT模式  1, ET模式
void modFd(int epoll_fd, int fd, int ev, int mode) {

    epoll_event event;
    event.data.fd = fd;

    if (mode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);

}
