
#ifndef CHENWEB_HTTP_CONN_H
#define CHENWEB_HTTP_CONN_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <mysql/mysql.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

#include "../global/global.h"
#include "../log/log.h"
#include "../pool/threadpool.h"
#include "../utils/utils.h"
#include "http_request.h"
#include "http_response.h"
#include "state_code.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    static int epoll_fd_;  // 所有的socket上的事件都被注册到同一个epoll对象上
    static int user_count_;  // 统计用户的数量

    // 初始化接受新的连接
    void Init(int sock_fd, const sockaddr_in& address, int trigger_mode);
    void Process();    // 响应以及处理客户端请求
    void CloseConn();  // 关闭连接
    bool Read();       // 非阻塞的读
    bool Write();      // 非阻塞的写
    inline sockaddr_in* getAddress() { return &address_; }

private:
    int sock_fd_;                     // 该http连接的socket
    int trigger_mode_;                // 触发组合模式
    sockaddr_in address_;             // 通信的socket地址
    char read_buf_[kReadBufferSize];  // 存储读取的请求报文数据
    int read_idx_;  // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    bool linger_;                       // 判断http请求是否要保持连接
    char write_buf_[kWriteBufferSize];  // 写缓冲区
    int write_idx_;                     // 写缓冲区中待发送的字节数
    int bytes_to_send_;                 // 将要发送的数据的字节数
    unsigned int bytes_have_send_;      // 已经发送的字节数
    struct stat file_stat_;             // 目标文件的状态。
    char* file_address_;  // 客户请求的目标文件被mmap到内存中的起始位置
    struct iovec iv_[2];          // 采用writev来执行写操作
    int iv_count_;                // 使用的块数
    HttpRequest http_request_;    // 解析请求内容
    HttpResponse http_response_;  // 生成响应

    void Init();
    void Unmap();  // 释放内存映射
};

#endif  // MYWEB_HTTP_CONN_H
