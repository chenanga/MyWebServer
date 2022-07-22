#include "http_conn.h"


HttpConn::HttpConn() {

}

HttpConn::~HttpConn() {

}

int HttpConn::m_user_count = 0;
int HttpConn::m_epoll_fd = -1;

// 响应以及处理客户端请求, 由线程池中的工作线程调用，处理http请求的入口函数
void HttpConn::process() {
    // 解析http请求
    m_http_request.init(m_read_buf, m_read_idx, &m_file_stat, &m_file_address);
    HTTP_CODE read_ret = m_http_request.parse_request();
    if (read_ret == NO_REQUEST) {
        modFd(m_epoll_fd, m_sock_fd, EPOLLIN, m_trigger_mode);
        return;
    }

    m_linger = m_http_request.get_m_linger();

    // 生成http响应
    m_http_response.init(&m_write_idx, m_write_buf, m_linger, m_http_request.get_m_real_file());
    bool write_ret = m_http_response.generate_response(read_ret, bytes_to_send, m_file_stat, m_iv[0], m_iv[1], m_file_address, m_iv_count);
    if (!write_ret)
        close_conn();
    modFd(m_epoll_fd, m_sock_fd, EPOLLOUT, m_trigger_mode);
}

// 初始化接受新的连接
void HttpConn::init(int sock_fd, const sockaddr_in &address, int trigger_mode) {
    m_sock_fd = sock_fd;
    m_address = address;
    m_trigger_mode = trigger_mode;

    // 设置端口复用
    int reuse = 1;
    setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    addFd(m_epoll_fd, sock_fd, true, m_trigger_mode);
    m_user_count ++;  // 总用户数加1

    init();
}

// 关闭连接
void HttpConn::close_conn() {
    if (m_sock_fd != -1) {
        removeFd(m_epoll_fd, m_sock_fd);
        m_sock_fd = -1;
        m_user_count --;  // 关闭一个连接，总数量减1
    }
}

// 非阻塞的读,循环读取客户数据，直到无数据可读或对方关闭连接
bool HttpConn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE)
        return false;

    // 读取到的字节
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sock_fd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)  // 没有数据
                break;
            return false;
        }
        else if (bytes_read == 0)
            // 对方关闭连接
            return false;

        m_read_idx += bytes_read;

    }
//    std::cout << "got a request ：" << m_read_buf << std::endl;
    return true;
}

// 一次写完所有数据
bool HttpConn::write() {
    int temp = 0;

    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modFd( m_epoll_fd, m_sock_fd, EPOLLIN, m_trigger_mode);
        init();
        return true;
    }

    while(1) {
        // 分散写
        temp = writev(m_sock_fd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                modFd( m_epoll_fd, m_sock_fd, EPOLLOUT, m_trigger_mode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        //第一个iovec头部信息的数据已发送完，发送第二个iovec数据
        if (bytes_have_send >= m_iv[0].iov_len) {
            //不再继续发送头部信息
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }

        //继续发送第一个iovec头部信息的数据
        else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0) {
            // 没有数据要发送了
            unmap();
            modFd(m_epoll_fd, m_sock_fd, EPOLLIN, m_trigger_mode);

            if (m_linger) {
                init();
                return true;
            }
            else {
                return false;
            }
        }
    }
}

void HttpConn::init() {
    m_read_idx = 0;
    m_linger = false;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', READ_BUFFER_SIZE);

}

void HttpConn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }

}