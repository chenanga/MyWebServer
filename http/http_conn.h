
#ifndef MYWEB_HTTP_CONN_H
#define MYWEB_HTTP_CONN_H

#include <unistd.h>
#include <csignal>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <sys/stat.h>
#include <cstring>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <cstdarg>
#include <cerrno>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <iostream>

#include "../utils/utils.h"
#include "../pool/threadpool.h"
#include "../global/global.h"
#include "state_code.h"
#include "http_request.h"
#include "http_response.h"



class HttpConn {
public:

    HttpConn();
    ~HttpConn();

    static int m_epoll_fd;  // 所有的socket上的事件都被注册到同一个epoll对象上
    static int m_user_count;  // 统计用户的数量

    void process();  // 响应以及处理客户端请求
    void init(int sock_fd, const sockaddr_in & address, int trigger_mode);  // 初始化接受新的连接
    void close_conn();  // 关闭连接

    bool read();  // 非阻塞的读,循环读取客户数据，直到无数据可读或对方关闭连接
    bool write();  // 非阻塞的写

private:
    int m_sock_fd;  // 该http连接的socket
    int m_trigger_mode;  // 触发组合模式
    sockaddr_in m_address;  // 通信的socket地址

    char m_read_buf[READ_BUFFER_SIZE];  // 存储读取的请求报文数据
    int m_read_idx;  // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置

    char m_write_buf[WRITE_BUFFER_SIZE];  // 写缓冲区
    int m_write_idx;                        // 写缓冲区中待发送的字节数

    int bytes_to_send;              // 将要发送的数据的字节数
    int bytes_have_send;            // 已经发送的字节数

    struct stat m_file_stat;     // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    char* m_file_address;        // 客户请求的目标文件被mmap到内存中的起始位置

    struct iovec m_iv[2];        // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。

    HttpRequest m_http_request;  // 解析请求内容
    HttpResponse m_http_response;  // 生成响应

    void init();
    void unmap();  // 释放内存映射

};


#endif //MYWEB_HTTP_CONN_H
