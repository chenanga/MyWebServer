
#ifndef CHENWEB_HTTP_STATE_CODE_H
#define CHENWEB_HTTP_STATE_CODE_H

/*
    服务器处理HTTP请求的可能结果，报文解析的结果
    NO_REQUEST          :   请求不完整，需要继续读取客户数据
    GET_REQUEST         :   表示获得了一个完成的客户请求
    BAD_REQUEST         :   表示客户请求语法错误
    NO_RESOURCE         :   表示服务器没有资源
    FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
    FILE_REQUEST        :   文件请求,获取文件成功
    INTERNAL_ERROR      :   表示服务器内部错误
    CLOSED_CONNECTION   :   表示客户端已经关闭连接了
*/
enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

// HTTP请求方法，目前支持GET、 POST
enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

/*
    解析客户端请求时，主状态机的状态
    CHECK_STATE_REQUESTLINE:当前正在分析请求行
    CHECK_STATE_HEADER:当前正在分析头部字段
    CHECK_STATE_CONTENT:当前正在解析请求体
*/
enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
};

// 从状态机的三种可能状态，即行的读取状态，分别表示
// 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

#endif  // CHENWEB_HTTP_STATE_CODE_H
