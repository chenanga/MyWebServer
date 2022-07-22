#include "http_response.h"

HttpResponse::HttpResponse() {

}

HttpResponse::~HttpResponse() {

}


void HttpResponse::init(int *write_idx, char *write_buf, bool linger, char *real_file) {
    m_write_buf = write_buf;
    m_write_idx = write_idx;
    m_linger = linger;
    m_real_file = real_file;
}


bool HttpResponse::generate_response(HTTP_CODE read_ret, int &bytes_to_send,
                                struct stat & m_file_stat, struct iovec & m_iv_0,struct iovec & m_iv_1,
                                        char *&file_address, int & m_iv_count) {

    switch (read_ret)
    {
        //内部错误，500
        case INTERNAL_ERROR:
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) {
                return false;
            }
            break;


        case BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)) {
                return false;
            }
            break;

            //报文语法有误，404

        case NO_RESOURCE:
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)) {
                return false;
            }
            break;

            //资源没有访问权限，403
        case FORBIDDEN_REQUEST:
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) {
                return false;
            }
            break;

            //文件存在，200
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);

            // 如果请求的文件不为空
            if (m_file_stat.st_size!=0) {

                // 打开文件
                int fd = open(m_real_file, O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    exit(-1);
                }

                file_address = (char *)mmap(0, (m_file_stat).st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                close(fd);

                add_headers(m_file_stat.st_size);
                //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx

                m_iv_0.iov_base = m_write_buf;
                m_iv_0.iov_len = *m_write_idx;
                //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
                m_iv_1.iov_base = file_address;
                m_iv_1.iov_len = m_file_stat.st_size;

                m_iv_count = 2;
                //发送的全部数据为响应报文头部信息和文件大小
                bytes_to_send = (*m_write_idx) + m_file_stat.st_size;

                return true;
            }
            else {
                // 如果请求资源大小为0，返回空白html文件
                const char* ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;
            }

        default:
            return false;
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv_0.iov_base = m_write_buf;
    m_iv_0.iov_len = *m_write_idx;
    m_iv_count = 1;
    bytes_to_send = *m_write_idx;
    return true;
}


// 往写缓冲中写入待发送的数据，可变参数
bool HttpResponse::add_response( const char* format, ... ) {

    //如果写入内容超出m_write_buf大小则报错
    if( *m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }

    //定义可变参数列表
    va_list arg_list;

    //将变量arg_list初始化为传入参数
    va_start( arg_list, format );

    //将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    // 从位置m_write_buf + m_write_idx 填入大小为WRITE_BUFFER_SIZE - 1 - m_write_idx，格式为format， 数据为arg_list的内容
    int len = vsnprintf( m_write_buf + *m_write_idx, WRITE_BUFFER_SIZE - 1 - *m_write_idx, format, arg_list );
    //如果写入的数据长度超过缓冲区剩余空间，则报错
    if( len >= ( WRITE_BUFFER_SIZE - 1 - *m_write_idx ) ) {
        va_end(arg_list);
        return false;
    }
    *m_write_idx += len;
    va_end( arg_list );
    return true;
}

// 响应首行
bool HttpResponse::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

// 响应头，这里简化了
bool HttpResponse::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool HttpResponse::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool HttpResponse::add_linger() {
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

// 增加一个空行
bool HttpResponse::add_blank_line() {
    return add_response( "%s", "\r\n" );
}

bool HttpResponse::add_content( const char* content ) {
    return add_response( "%s", content );
}

bool HttpResponse::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

