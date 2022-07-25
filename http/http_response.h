
#ifndef CHENWEB_HTTP_RESPONSE_H
#define CHENWEB_HTTP_RESPONSE_H

#include <sys/stat.h>  // stat
#include <fcntl.h>  // iovec
#include <cstring>
#include <string>
#include <cstdarg>
#include <sys/mman.h> // mmap, PROT_READ, MAP_PRIVATE
#include <unistd.h>

#include "state_code.h"
#include "../global/global.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(int * write_idx, char *write_buf, bool linger, char *real_file);
    bool generate_response(HTTP_CODE read_ret, int &bytes_to_send,
                           struct stat & m_file_stat, struct iovec & m_iv_0, struct iovec & m_iv_1,
                                   char * &file_address, int & m_iv_count);

private:
    int * m_write_idx;  // 指针接受从http_conn传递来的参数
    char * m_write_buf;
    bool m_linger; // 判断http请求是否要保持连接
    char * m_real_file;  // 请求的文件名

    // 这一组函数被process_write调用以填充HTTP应答。
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();
};


#endif //CHENWEB_HTTP_RESPONSE_H
