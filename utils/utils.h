
#ifndef CHENWEB_UTILS_UTILS_H
#define CHENWEB_UTILS_UTILS_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <csignal>
#include <cstring>

#include "../log/log.h"

// 添加信号捕捉
void AddSig(int sig, void(handler)(int));

// 添加文件描述符到epoll中, mode: 0, LT模式  1, ET模式
void AddFd(int epoll_fd, int fd, bool one_shot, int mode);

// 从内核事件表中删除描述符
void RemoveFd(int epoll_fd, int fd);

// 修改文件描述符, mode: 0, LT模式  1, ET模式
void ModifyFd(int epoll_fd, int fd, int ev, int mode);

// 设置文件描述符非阻塞
void SetNoneBlocking(int fd);

#endif  // CHENWEB_UTILS_UTILS_H
