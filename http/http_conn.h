
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


class Http_conn {
public:

    Http_conn();
    ~Http_conn();

    static int m_epoll_fd;  // 所有的socket上的事件都被注册到同一个epoll对象上
    static int m_user_count;  // 统计用户的数量


    void process();  // 响应以及处理客户端请求


private:
    int m_sock_fd;  // 该http连接的socket
    sockaddr_in m_address;  // 通信的socket地址

};


#endif //MYWEB_HTTP_CONN_H
