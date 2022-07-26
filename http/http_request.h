
#ifndef CHENWEB_HTTP_REQUEST_H
#define CHENWEB_HTTP_REQUEST_H

#include <cstring>
#include <iostream>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>  // stat
#include <fcntl.h>  // iovec
#include <unistd.h>
#include <sys/mman.h> // mmap, PROT_READ, MAP_PRIVATE
#include <mysql/mysql.h>


#include "state_code.h"
#include "../global/global.h"
#include "../lock/locker.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"



void initmysql_result(SqlConnPool *connPool);


class HttpRequest {

public:
    HttpRequest();
    ~HttpRequest();

    void init();  // 初始化一些信息
    void init(char *read_buf, int read_idx, struct stat * file_stat, char ** file_address);
    HTTP_CODE parse_request();  // 解析http请求
    inline bool get_m_linger() const { return m_linger; };  // 是http_conn获取解析后的m_linger
    inline char * get_m_real_file() { return m_real_file; }
    MYSQL * m_mysql;

private:
    inline char * get_line() { return m_read_buf + m_start_line; };  // 内联函数, 执行速度更快

    HTTP_CODE parse_request_line(char * text); // 解析请求首行, 获得请求方法、目标url, HTTP版本
    HTTP_CODE parse_request_headers(char * text); // 解析请求头
    HTTP_CODE parse_request_content(char * text); // 解析请求内容
    LINE_STATUS parse_line(); // 获取一行, 判断依据\r\n

    HTTP_CODE do_request();



private:

    char * m_read_buf;  // 存储读取的请求报文数据
    int m_read_idx;  // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int m_checked_idx;  // m_read_buf读取的位置m_checked_idx
    int m_start_line; // 当前正在解析的行的起始位置
    char * m_url; // 请求目标文件的文件名
    char * m_version; // 协议版本，支持 HTTP1.1
    METHOD m_method; // 请求方法
    char * m_host; // 主机名
    int m_content_length; //HTTP请求的消息总长度
    bool m_linger; // 判断http请求是否要保持连接

    bool m_ispost;  // 是否为post请求
    char *m_string; // 储存请求数据部分
    char m_real_file[FILENAME_LEN];       // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char ** m_file_address;                   // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat * m_file_stat;                // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    int m_iv_count;  //被写内存块的数量

    CHECK_STATE m_check_state; // 主状态机当前所处的状态


};

#endif //CHENWEB_HTTP_REQUEST_H
