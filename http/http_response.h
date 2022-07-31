
#ifndef CHENWEB_HTTP_HTTP_RESPONSE_H
#define CHENWEB_HTTP_HTTP_RESPONSE_H

#include <fcntl.h>     // iovec
#include <sys/mman.h>  // mmap, PROT_READ, MAP_PRIVATE
#include <sys/stat.h>  // stat
#include <unistd.h>

#include <cstdarg>
#include <cstring>
#include <string>
#include <unordered_map>

#include "../global/global.h"
#include "../log/log.h"
#include "state_code.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(int *write_idx, char *write_buf, bool linger, char *real_file);
    bool generate_response(HTTP_CODE read_ret, int &bytes_to_send,
                           struct stat &file_stat, struct iovec &iv_0,
                           struct iovec &iv_1, char *&file_address,
                           int &iv_count);

private:
    int *write_idx_;   // 指针接受从http_conn传递来的参数
    char *write_buf_;  // 写缓冲
    bool linger_;      // 判断http请求是否要保持连接
    char *real_file_;  // 请求的文件名

    // 这一组函数被process_write调用以填充HTTP应答。
    bool AddResponse(const char *format, ...);
    bool AddContent(const char *content);
    bool AddContentType();
    bool AddStatusLine(int status, const char *title);
    void AddHeaders(int content_length);
    bool AddContentLength(int content_length);
    bool AddLinger();
    bool AddBlankLine();
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    std::string GetFileType();
};

#endif  // CHENWEB_HTTP_HTTP_RESPONSE_H
