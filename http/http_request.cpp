#include "http_request.h"

// 将表中的用户名和密码放入map
std::map<std::string, std::string> g_users;
Locker g_lock_map;  // 多线程操作时候使用

void InitMysqlResult() {
    // 先从连接池中取一个连接
    MYSQL *mysql = nullptr;
    SqlConnRAII mysql_coon(&mysql, SqlConnPool::GetInstance());

    // 在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,password FROM user"))
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));

    MYSQL_RES *result = mysql_store_result(mysql);  // 从表中检索完整的结果集
    /* MYSQL_FIELD *fields = mysql_fetch_fields(result);  //
     * 返回所有字段结构的数组 */

    // 从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        g_users[temp1] = temp2;
    }
    mysql_free_result(result);
    SqlConnPool::GetInstance()->ReleaseConnection(mysql);
}

HttpRequest::HttpRequest() { Init(); }

HttpRequest::~HttpRequest() {}

void HttpRequest::Init() {
    check_state_ = CHECK_STATE_REQUESTLINE;  // 初始化状态为请求首行
    checked_idx_ = 0;
    read_idx_ = 0;
    start_line_ = 0;
    url_ = nullptr;
    version_ = nullptr;
    method_ = GET;
    linger_ = false;
    is_post_ = false;
    mysql_ = nullptr;
    memset(real_file_, '\0', kFileNameLen);
}

void HttpRequest::Init(char *read_buf, int read_idx, struct stat *file_stat,
                       char **file_address, sockaddr_in *client_address) {
    Init();
    read_buf_ = read_buf;
    read_idx_ = read_idx;
    file_stat_ = file_stat;
    file_address_ = file_address;
    client_address_ = client_address;
}

HTTP_CODE HttpRequest::ParseRequest() {
    // 初始化从状态机状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = nullptr;
    /*
    在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=ParseLine())==LINE_OK语句即可。
    但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。
    解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，也就是说，符合循环入口条件，还会再次进入循环，
    这并不是我们所希望的。为此，增加了该语句&& line_status ==
    LINE_OK，并在完成消息体解析后，将line_status变量更改为LINE_OPEN，
    此时可以跳出循环，完成报文解析任务。
    */

    // 解析到了一行完整的数据，或者解析到了请求体，也是完整的数据
    while (
        ((check_state_ == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) ||
        ((line_status = ParseLine()) == LINE_OK)) {
        text = GetLine();  // 获取一行数据

        /*         m_checked_idx表示从状态机在m_read_buf中读取的位置
                 m_start_line是每一个数据行在m_read_buf中的起始位置*/
        start_line_ = checked_idx_;

        switch (check_state_) {
            case CHECK_STATE_REQUESTLINE: {
                ret = ParseRequestLine(text);  // 解析请求行
                if (ret == BAD_REQUEST) return BAD_REQUEST;
                break;
            }

            case CHECK_STATE_HEADER: {
                ret = ParseRequestHeaders(text);  // 解析请求头
                if (ret ==
                    BAD_REQUEST)  // 目前这个if条件始终无法为真，因为parse_request_headers所有的返回情况没有返回BAD_REQUEST
                    return BAD_REQUEST;
                else if (
                    ret ==
                    GET_REQUEST) {  // 完整解析GET请求后，跳转到报文响应函数
                    return DoRequest();
                }
                break;
            }

            case CHECK_STATE_CONTENT: {
                ret = ParseRequestContent(text);  // 解析消息体
                if (ret == GET_REQUEST)
                    return DoRequest();  // 完整解析POST请求后，跳转到报文响应函数
                line_status =
                    LINE_OPEN;  // 解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

LINE_STATUS HttpRequest::ParseLine() {
    char temp;
    for (; checked_idx_ < read_idx_; ++checked_idx_) {
        temp = read_buf_[checked_idx_];
        if (temp == '\r') {
            // 表示当时数组位置已到数组末尾，但是只检测到\r，没有检测到到\n，不完整的
            if (checked_idx_ + 1 == read_idx_)
                return LINE_OPEN;
            else if (read_buf_[checked_idx_ + 1] == '\n') {
                read_buf_[checked_idx_++] = '\0';  // \r 替换为 \0
                read_buf_[checked_idx_++] = '\0';  // \n 替换为 \0
                return LINE_OK;
            }
            return LINE_BAD;
        }

        // 当前字节不是\r，判断是否是\n（一般是上次读取到\r就到了buffer末尾，没有接收完整，再次接收时会出现这种情况）
        else if (temp == '\n') {
            // 前一个字符是\r，则接收完整
            if (checked_idx_ >= 1 && read_buf_[checked_idx_ - 1] == '\r') {
                read_buf_[checked_idx_ - 1] = '\0';  // \r 替换为 \0
                read_buf_[checked_idx_++] = '\0';    // \n 替换为 \0
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CODE HttpRequest::ParseRequestLine(char *text) {
    /*     GET /index.html HTTP/1.1
         请求行中最先含有空格和\t任一字符的位置并返回*/
    url_ = strpbrk(text, " \t");

    if (!url_) return BAD_REQUEST;

    // GET\0/index.html HTTP/1.1
    *url_++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        method_ = GET;
    else if (strcasecmp(method, "POST") == 0) {
        method_ = POST;
        is_post_ = true;
    } else
        return BAD_REQUEST;

    // /index.html HTTP/1.1  判断HTTP版本号
    version_ = strpbrk(url_, " \t");
    if (!version_) return BAD_REQUEST;

    // /index.html\0HTTP/1.1
    *version_++ = '\0';
    if (strcasecmp(version_, "HTTP/1.1") != 0)  // 仅支持HTTP/1.1
        return BAD_REQUEST;

    /*     http://192.168.1.1:10000/index.html
         //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理*/
    if (strncasecmp(url_, "http://", 7) == 0) {
        url_ += 7;                 // 192.168.1.1:10000/index.html
        url_ = strchr(url_, '/');  // /index.html
    }

    // 同样增加https情况
    if (strncasecmp(url_, "https://", 8) == 0) {
        url_ += 8;
        url_ = strchr(url_, '/');
    }

    // 一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
    if (!url_ || url_[0] != '/') return BAD_REQUEST;

    // 如果直接访问ip:端口/，则跳转到初始界面,即当url为/时，显示欢迎界面
    if (strlen(url_) == 1) strcat(url_, "4");
    check_state_ = CHECK_STATE_HEADER;  // 主状态机检查状态变成请求头
    return NO_REQUEST;
}

HTTP_CODE HttpRequest::ParseRequestHeaders(char *text) {
    /*     通过\n\r划分为一行一行的(如果当前位置为\n\r，下一个位置也是\n\r，说明到了请求头结束的地方)
         通过 ": " 划分key和value
         遇到空行，表示头部字段解析完毕*/
    if (text[0] == '\0') {
        /*         如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
                 状态机转移到CHECK_STATE_CONTENT状态*/
        if (content_length_ != 0) {
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    // 处理Connection 头部字段  Connection: keep-alive
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            linger_ = true;
        }
    }

    // 处理Content-Length头部字段
    else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    }

    // 处理Host头部字段
    else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    }

    else
        LOG_WARN("oop! unknow header: %s", text);

    return NO_REQUEST;
}

HTTP_CODE HttpRequest::ParseRequestContent(char *text) {
    // 没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
    if (read_idx_ >= (content_length_ + checked_idx_)) {
        text[content_length_] = '\0';
        request_data_ = text;
        // POST请求中最后为输入的用户名和密码
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CODE HttpRequest::DoRequest() {
    // kStrCurDir 为程序运行路径，一般为项目根目录，网站文件存放在resources中
    strcpy(real_file_, std::string(kStrCurDir + "/resources").c_str());
    int len = strlen(std::string(kStrCurDir + "/resources").c_str());
    const char *p = strrchr(url_, '/');
    if (p == nullptr) {
        perror("strrchr");
        exit(-1);
    }

    // 处理post请求 且是登录或者注册
    if (is_post_ == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, url_ + 2);
        strncpy(real_file_ + len, m_url_real, kFileNameLen - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来, user=abcd&password=asdasd
        char name[100], password[100];
        int name_index;
        for (name_index = 5; request_data_[name_index] != '&'; ++name_index)
            name[name_index - 5] = request_data_[name_index];
        name[name_index - 5] = '\0';

        int passwd_index = 0;  // 此时i在&这里，加10刚好到密码的第一个字符
        for (name_index = name_index + 10; request_data_[name_index] != '\0';
             ++name_index, ++passwd_index)
            password[passwd_index] = request_data_[name_index];
        password[passwd_index] = '\0';

        if (*(p + 1) == '3') {
            // 注册，先检测数据库中是否有重名的, 没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            if (g_users.find(name) == g_users.end()) {
                g_lock_map.lock();
                // 当有用户注册时候，再去取链接
                SqlConnRAII mysql_coon(&mysql_, SqlConnPool::GetInstance());
                int res = mysql_query(mysql_, sql_insert);
                free(sql_insert);
                g_lock_map.unlock();

                if (!res) {
                    g_lock_map.lock();
                    g_users.insert(
                        std::pair<std::string, std::string>(name, password));
                    g_lock_map.unlock();

                    strcpy(url_, "/log.html");
                    LOG_INFO(
                        "新用户注册成功：user = %s, password = %s, "
                        "client_address = %s",
                        name, password, inet_ntoa(client_address_->sin_addr));
                } else {
                    strcpy(url_, "/registerError.html");
                    LOG_INFO(
                        "新用户注册失败，数据库写入错误：user = %s, password = "
                        "%s, client_address = %s",
                        name, password, inet_ntoa(client_address_->sin_addr));
                }
            } else {
                strcpy(url_, "/registerError.html");
                LOG_INFO(
                    "新用户注册失败，用户名重复：user = %s, password = %s, "
                    "client_address = %s",
                    name, password, inet_ntoa(client_address_->sin_addr));
            }
        }
        /*         如果是登录，直接判断
                 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0*/
        else if (*(p + 1) == '2') {
            if (g_users.find(name) != g_users.end() &&
                g_users[name] == password) {
                strcpy(url_, "/welcome.html");

                LOG_INFO(
                    "用户登录成功：user = %s, password = %s, client_address = "
                    "%s",
                    name, password, inet_ntoa(client_address_->sin_addr));

            } else {
                strcpy(url_, "/logError.html");
                LOG_INFO(
                    "用户登录失败，账号或密码错误：user = %s, password = %s, "
                    "client_address = %s",
                    name, password, inet_ntoa(client_address_->sin_addr));
            }
        }
    }

    /*     访问端口号或者index.html界面,
         因为默认是访问这个界面，所有放在if第一行，提高命中率，减少if判断次数*/
    if (*(p + 1) == '8') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/test.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));  //拼接路径
        free(m_url_real);
    } else if (*(p + 1) == '4') {  // 默认界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/index.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));  //拼接路径
        free(m_url_real);
    } else if (*(p + 1) == '1') {  // 访问登录界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    } else if (*(p + 1) == '0') {  // 访问注册界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));  //拼接路径
        free(m_url_real);
    } else if (*(p + 1) == '5') {  // 访问图片界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    } else if (*(p + 1) == '6') {  // 访问视频界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }

    else if (*(p + 1) == '7') {  // 访问关注界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(real_file_ + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    } else  // 其他界面,跳转到index
        strncpy(real_file_ + len, url_, kFileNameLen - len - 1);

    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if (stat(real_file_, file_stat_) < 0) return NO_RESOURCE;

    // 判断访问权限
    if (!((*file_stat_).st_mode & S_IROTH)) return FORBIDDEN_REQUEST;

    // 判断是否是目录
    if (S_ISDIR((*file_stat_).st_mode)) return BAD_REQUEST;

    return FILE_REQUEST;
}
