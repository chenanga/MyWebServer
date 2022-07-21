#include "http_conn.h"


Http_conn::Http_conn() {

}

Http_conn::~Http_conn() {

}

int Http_conn::m_user_count = 0;
int Http_conn::m_epoll_fd = -1;

// 响应以及处理客户端请求, 有线程池中的工作线程带哦用，处理http请求的入口函数
void Http_conn::process() {
    // 解析http请求

    // 生成http响应
}

// 初始化接受新的连接
void Http_conn::init(int sock_fd, const sockaddr_in &address, int trigger_mode) {
    m_sock_fd = sock_fd;
    m_address = address;
    m_trigger_mode = trigger_mode;

    // 设置端口复用
    int reuse = 1;
    setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    addFd(m_epoll_fd, sock_fd, true, m_trigger_mode);
    m_user_count ++;  // 总用户数加1
}

// 关闭连接
void Http_conn::close_conn() {
    if (m_sock_fd != -1) {
        removeFd(m_epoll_fd, m_sock_fd);
        m_sock_fd = -1;
        m_user_count --;  // 关闭一个连接，总数量减1
    }
}

bool Http_conn::read() {
    std::cout << "一次性读取所有数据" << std::endl;
    return true;
}

bool Http_conn::write() {
    std::cout << "一次性写完所有数据" << std::endl;
    return true;
}
