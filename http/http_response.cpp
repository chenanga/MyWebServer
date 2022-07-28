#include "http_response.h"

HttpResponse::HttpResponse() {}

HttpResponse::~HttpResponse() {}

void HttpResponse::init(int *write_idx, char *write_buf, bool linger,
                        char *real_file) {
    write_buf_ = write_buf;
    write_idx_ = write_idx;
    linger_ = linger;
    real_file_ = real_file;
}

bool HttpResponse::generate_response(HTTP_CODE read_ret, int &bytes_to_send,
                                     struct stat &file_stat, struct iovec &iv_0,
                                     struct iovec &iv_1, char *&file_address,
                                     int &iv_count) {
    switch (read_ret) {
        // 内部错误，500
        case INTERNAL_ERROR:
            AddStatusLine(500, error_500_title);
            AddHeaders(strlen(error_500_form));
            if (!AddContent(error_500_form)) return false;
            break;

        case BAD_REQUEST:
            AddStatusLine(400, error_400_title);
            AddHeaders(strlen(error_400_form));
            if (!AddContent(error_400_form)) return false;
            break;

            // 报文语法有误，404
        case NO_RESOURCE:
            AddStatusLine(404, error_404_title);
            AddHeaders(strlen(error_404_form));
            if (!AddContent(error_404_form)) return false;
            break;

            // 资源没有访问权限，403
        case FORBIDDEN_REQUEST:
            AddStatusLine(403, error_403_title);
            AddHeaders(strlen(error_403_form));
            if (!AddContent(error_403_form)) return false;
            break;

            // 文件存在，200
        case FILE_REQUEST:
            AddStatusLine(200, ok_200_title);

            // 如果请求的文件不为空
            if (file_stat.st_size != 0) {
                // 打开文件
                int fd = open(real_file_, O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    exit(-1);
                }

                file_address = (char *)mmap(0, (file_stat).st_size, PROT_READ,
                                            MAP_PRIVATE, fd, 0);
                close(fd);
                AddHeaders(file_stat.st_size);

                iv_0.iov_base = write_buf_;  // 指针指向响应报文缓冲区
                iv_0.iov_len = *write_idx_;  // 长度指向m_write_idx
                iv_1.iov_base = file_address;  // 指针指向mmap返回的文件指针
                iv_1.iov_len = file_stat.st_size;  // 长度指向文件大小
                iv_count = 2;
                bytes_to_send =
                    (*write_idx_) +
                    file_stat
                        .st_size;  // 发送的全部数据为响应报文头部信息和文件大小
                return true;
            } else {
                // 如果请求资源大小为0，返回空白html文件
                const char *ok_string = "<html><body></body></html>";
                AddHeaders(strlen(ok_string));
                if (!AddContent(ok_string)) return false;
            }

        default:
            return false;
    }
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    iv_0.iov_base = write_buf_;
    iv_0.iov_len = *write_idx_;
    iv_count = 1;
    bytes_to_send = *write_idx_;
    return true;
}

// 往写缓冲中写入待发送的数据，可变参数
bool HttpResponse::AddResponse(const char *format, ...) {
    // 如果写入内容超出m_write_buf大小则报错
    if (*write_idx_ >= kWriteBufferSize) return false;

    va_list arg_list;            // 定义可变参数列表
    va_start(arg_list, format);  // 将变量arg_list初始化为传入参数

    /*    将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
         从位置m_write_buf + write_idx_ 填入大小为WRITE_BUFFER_SIZE - 1 -
       *write_idx_，格式为format， 数据为arg_list的内容*/
    int len = vsnprintf(write_buf_ + *write_idx_,
                        kWriteBufferSize - 1 - *write_idx_, format, arg_list);
    //如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (kWriteBufferSize - 1 - *write_idx_)) {
        va_end(arg_list);
        return false;
    }
    *write_idx_ += len;
    va_end(arg_list);
    return true;
}

// 响应首行
bool HttpResponse::AddStatusLine(int status, const char *title) {
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 响应头，这里简化了
bool HttpResponse::AddHeaders(int content_length) {
    AddContentLength(content_length);
    AddContentType();
    AddLinger();
    AddBlankLine();
}

bool HttpResponse::AddContentLength(int content_length) {
    return AddResponse("Content-Length: %d\r\n", content_length);
}

bool HttpResponse::AddLinger() {
    return AddResponse("Connection: %s\r\n",
                       (linger_ == true) ? "keep-alive" : "close");
}

bool HttpResponse::AddBlankLine() { return AddResponse("%s", "\r\n"); }

bool HttpResponse::AddContent(const char *content) {
    return AddResponse("%s", content);
}

bool HttpResponse::AddContentType() {
    return AddResponse("Content-Type:%s\r\n", "text/html");
}
