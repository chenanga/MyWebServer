#include "http_conn.h"

HttpConn::HttpConn() {}

HttpConn::~HttpConn() {}

int HttpConn::user_count_ = 0;
int HttpConn::epoll_fd_ = -1;

// 响应以及处理客户端请求, 由线程池中的工作线程调用，处理http请求的入口函数
void HttpConn::Process() {
    // 解析http请求
    http_request_.Init(read_buf_, read_idx_, &file_stat_, &file_address_,
                       getAddress());
    HTTP_CODE read_ret = http_request_.ParseRequest();
    if (read_ret == NO_REQUEST) {
        ModifyFd(epoll_fd_, sock_fd_, EPOLLIN, trigger_mode_);
        return;
    }
    linger_ = http_request_.getLinger();
    // 生成http响应
    http_response_.init(&write_idx_, write_buf_, linger_,
                        http_request_.getRealFile());
    bool write_ret = http_response_.generate_response(
        read_ret, bytes_to_send_, file_stat_, iv_[0], iv_[1], file_address_,
        iv_count_);
    if (!write_ret) CloseConn();
    ModifyFd(epoll_fd_, sock_fd_, EPOLLOUT, trigger_mode_);
}

// 初始化接受新的连接
void HttpConn::Init(int sock_fd, const sockaddr_in &address, int trigger_mode) {
    sock_fd_ = sock_fd;
    address_ = address;
    trigger_mode_ = trigger_mode;

    // 设置端口复用
    int reuse = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    AddFd(epoll_fd_, sock_fd, true, trigger_mode_);
    user_count_++;  // 总用户数加1
    Init();
}

// 关闭连接
void HttpConn::CloseConn() {
    if (sock_fd_ != -1) {
        RemoveFd(epoll_fd_, sock_fd_);
        sock_fd_ = -1;
        user_count_--;  // 关闭一个连接，总数量减1
    }
}

// 非阻塞的读,循环读取客户数据，直到无数据可读或对方关闭连接
bool HttpConn::Read() {
    if (read_idx_ >= kReadBufferSize) return false;

    // 读取到的字节
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(sock_fd_, read_buf_ + read_idx_,
                          kReadBufferSize - read_idx_, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)  // 没有数据
                break;
            return false;
        } else if (bytes_read == 0)
            // 对方关闭连接
            return false;
        read_idx_ += bytes_read;
    }
    return true;
}

// 一次写完所有数据
bool HttpConn::Write() {
    int temp = 0;

    // 将要发送的字节为0，这一次响应结束。
    if (bytes_to_send_ == 0) {
        ModifyFd(epoll_fd_, sock_fd_, EPOLLIN, trigger_mode_);
        Init();
        return true;
    }

    while (1) {
        // 分散写
        temp = writev(sock_fd_, iv_, iv_count_);
        if (temp <= -1) {
            /*             如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
                         服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。*/
            if (errno == EAGAIN) {
                ModifyFd(epoll_fd_, sock_fd_, EPOLLOUT, trigger_mode_);
                return true;
            } else if (errno == EINTR)
                continue;
            Unmap();
            return false;
        }
        bytes_have_send_ += temp;
        bytes_to_send_ -= temp;

        // 第一个iovec头部信息的数据已发送完，发送第二个iovec数据
        if (bytes_have_send_ >= iv_[0].iov_len) {
            // 不再继续发送头部信息
            iv_[0].iov_len = 0;
            iv_[1].iov_base = file_address_ + (bytes_have_send_ - write_idx_);
            iv_[1].iov_len = bytes_to_send_;
        }

        // 继续发送第一个iovec头部信息的数据
        else {
            iv_[0].iov_base = write_buf_ + bytes_have_send_;
            iv_[0].iov_len = iv_[0].iov_len - bytes_have_send_;
        }

        if (bytes_to_send_ <= 0) {
            // 没有数据要发送了
            Unmap();
            ModifyFd(epoll_fd_, sock_fd_, EPOLLIN, trigger_mode_);

            if (linger_) {
                Init();
                return true;
            } else
                return false;
        }
    }
}

void HttpConn::Init() {
    read_idx_ = 0;
    linger_ = false;
    memset(read_buf_, '\0', kReadBufferSize);
    memset(write_buf_, '\0', kReadBufferSize);
}

void HttpConn::Unmap() {
    if (file_address_) {
        munmap(file_address_, file_stat_.st_size);
        file_address_ = 0;
    }
}