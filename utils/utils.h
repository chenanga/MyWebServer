
#ifndef MYWEB_UTILS_H
#define MYWEB_UTILS_H


#include <csignal>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include "../log/log.h"


// 添加信号捕捉
void addSig(int sig, void(handler)(int));

// 添加文件描述符到epoll中, mode: 0, LT模式  1, ET模式
void addFd(int epoll_fd, int fd, bool one_shot, int mode);

// 从内核事件表中删除描述符
void removeFd(int epollfd, int fd);

// 修改文件描述符, mode: 0, LT模式  1, ET模式
void modFd(int epollfd, int fd, int ev, int mode);

void setNoneBlocking(int fd);  // 设置文件描述符非阻塞


#endif //MYWEB_UTILS_H
