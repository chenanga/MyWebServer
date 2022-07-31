
#ifndef CHENWEB_HTTP_HTTP_REQUEST_H
#define CHENWEB_HTTP_HTTP_REQUEST_H

#include <arpa/inet.h>
#include <fcntl.h>  // iovec
#include <mysql/mysql.h>
#include <netinet/in.h>
#include <sys/mman.h>  // mmap, PROT_READ, MAP_PRIVATE
#include <sys/stat.h>  // stat
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <map>

#include "../global/global.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "state_code.h"

void InitMysqlResult();

class HttpRequest {
public:
    HttpRequest();
    ~HttpRequest();

    void Init();  // 初始化一些信息
    void Init(char *read_buf, int read_idx, struct stat *file_stat,
              char **file_address, sockaddr_in *client_address);
    HTTP_CODE ParseRequest();  // 解析http请求
    inline bool getLinger() const { return linger_; };
    inline char *getRealFile() { return real_file_; }
    MYSQL *mysql_;
    inline int getReadIdx() { return read_idx_; };

private:
    inline char *GetLine() { return read_buf_ + start_line_; };

    HTTP_CODE ParseRequestLine(char *text);     // 解析请求首行
    HTTP_CODE ParseRequestHeaders(char *text);  // 解析请求头
    HTTP_CODE ParseRequestContent(char *text);  // 解析请求内容
    LINE_STATUS ParseLine();  // 获取一行, 判断依据\r\n
    HTTP_CODE DoRequest();    // 处理请求

private:
    char *read_buf_;  // 存储读取的请求报文数据
    int read_idx_;  // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int checked_idx_;               // m_read_buf读取的位置m_checked_idx
    int start_line_;                // 当前正在解析的行的起始位置
    char *url_;                     // 请求目标文件的文件名
    char real_file_[kFileNameLen];  // 客户请求的目标文件的完整路径
    char **file_address_;  // 客户请求的目标文件被mmap到内存中的起始位置
    char *request_data_;  // 储存请求头数据部分
    struct stat *
        file_stat_;  // 目标文件的状态。是否存在、是否为目录、是否可读，并获取文件大小等信息
    int iv_count_;   // 被写内存块的数量
    char *version_;  // 协议版本，支持 HTTP1.1
    METHOD method_;  // 请求方法
    char *host_;     // 主机名
    int content_length_;           // HTTP请求的消息总长度
    bool linger_;                  // 判断http请求是否要保持连接
    sockaddr_in *client_address_;  // 客户通信地址
    bool is_post_;                 // 是否为post请求
    CHECK_STATE check_state_;      // 主状态机当前所处的状态
};

#endif  // CHENWEB_HTTP_HTTP_REQUEST_H
