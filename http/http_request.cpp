#include "http_request.h"


std::map<std::string, std::string> users;
Locker m_lock_map;  // 多线程操作时候使用

HttpRequest::HttpRequest() {

    init();
}

HttpRequest::~HttpRequest() {

}

void HttpRequest::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;  // 初始化状态为请求首行
    m_checked_idx = 0;
    m_start_line = 0;
    m_url = nullptr;  // todo 如果正常后，试试吧0换成nullptr
    m_version = nullptr;
    m_method = GET;
    m_linger = false;
    m_ispost = false;
    memset(m_real_file, '\0', FILENAME_LEN);
}

void HttpRequest::init(char *read_buf, int read_idx, struct stat * file_stat, char ** file_address) {
    m_read_buf = read_buf;
    m_read_idx = read_idx;
    m_file_stat = file_stat;
    m_file_address = file_address;
    init();
}

HTTP_CODE HttpRequest::parse_request() {

    //初始化从状态机状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char * text = nullptr;

    /*
    在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。
    但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。

    解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，也就是说，符合循环入口条件，还会再次进入循环，
    这并不是我们所希望的。为此，增加了该语句&& line_status == LINE_OK，并在完成消息体解析后，将line_status变量更改为LINE_OPEN，
    此时可以跳出循环，完成报文解析任务。
    */

    // 解析到了一行完整的数据，或者解析到了请求体，也是完整的数据
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
           || ((line_status = parse_line()) == LINE_OK)) {

        text = get_line();  // 获取一行数据
//        std::cout << "got 1 http line : " << text << std::endl;
        // m_checked_idx表示从状态机在m_read_buf中读取的位置
        // m_start_line是每一个数据行在m_read_buf中的起始位置
        m_start_line = m_checked_idx;

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);  //解析请求行
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }

            case CHECK_STATE_HEADER: {
                ret = parse_request_headers(text);  //解析请求头
                if (ret == BAD_REQUEST)  // 目前这个if条件始终无法为真，因为parse_request_headers所有的返回情况没有返回BAD_REQUEST
                    return BAD_REQUEST;

                else if (ret == GET_REQUEST) {    //完整解析GET请求后，跳转到报文响应函数
                    return do_request();
                }
                break;
            }

            case CHECK_STATE_CONTENT: {
                ret = parse_request_content(text);   //解析消息体
                if (ret == GET_REQUEST)
                    return do_request();       //完整解析POST请求后，跳转到报文响应函数

                line_status = LINE_OPEN;     //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                break;
            }
            default:
                return INTERNAL_ERROR;

        }
    }

    return NO_REQUEST;
}

LINE_STATUS HttpRequest::parse_line() {
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++ m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) //表示当时数组位置已到数组末尾，但是只检测到\r，没有检测到到\n，不完整的
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx ++] = '\0';  // \r 替换为 \0
                m_read_buf[m_checked_idx ++] = '\0';  // \n 替换为 \0
                return LINE_OK;
            }
            return LINE_BAD;
        }

        else if (temp == '\n') { // 当前字节不是\r，判断是否是\n（一般是上次读取到\r就到了buffer末尾，没有接收完整，再次接收时会出现这种情况）
            if (m_checked_idx >= 1 && m_read_buf[m_checked_idx - 1] == '\r') { // 前一个字符是\r，则接收完整
                m_read_buf[m_checked_idx - 1] = '\0';  // \r 替换为 \0
                m_read_buf[m_checked_idx ++] = '\0';  // \n 替换为 \0
                return LINE_OK;
            }
            return LINE_BAD;
        }

    }
    return LINE_OPEN;
}

HTTP_CODE HttpRequest::parse_request_line(char *text) {
    // GET /index.html HTTP/1.1
    // 请求行中最先含有空格和\t任一字符的位置并返回
    m_url = strpbrk(text, " \t"); // strpbrk是在源字符串（s1）中找出最先含有搜索字符串（s2）中任一字符的位置并返回, 只要包含空格或者\t就返回
    if (!m_url) {
        return BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';

    char * method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0) {
        m_method = POST;
        m_ispost = true;
    }
    else
        return BAD_REQUEST;

    // /index.html HTTP/1.1  判断HTTP版本号
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;

    // /index.html\0HTTP/1.1
    *m_version ++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0)  //仅支持HTTP/1.1
        return BAD_REQUEST;

    // http://192.168.1.1:10000/index.html  //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7; // 192.168.1.1:10000/index.html
        m_url = strchr(m_url, '/'); // /index.html
    }

    //同样增加https情况
    if(strncasecmp(m_url,"https://",8)==0)
    {
        m_url+=8;
        m_url=strchr(m_url,'/');
    }

    //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    // 如果直接访问ip:端口/，则跳转到初始界面,即当url为/时，显示欢迎界面, 4 为欢迎界面
    if (strlen(m_url) == 1)
        strcat(m_url, "4");
    m_check_state = CHECK_STATE_HEADER; // 主状态机检查状态变成请求头

    return NO_REQUEST;
}

HTTP_CODE HttpRequest::parse_request_headers(char *text) {
    // 通过\n\r划分为一行一行的(如果当前位置为\n\r，下一个位置也是\n\r，说明到了请求头结束的地方)
    // 通过 ": " 划分key和value
    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {

        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    }

    else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    }

    else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    }

    else if (strncasecmp(text, "Host:", 5) == 0) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    }

    else {
//        std::cout << "oop! unknow header: " << text << std::endl;
    }
    return NO_REQUEST;
}

HTTP_CODE HttpRequest::parse_request_content(char *text) {
    //没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';
        m_string = text;
        //POST请求中最后为输入的用户名和密码
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CODE HttpRequest::do_request() {
    // g_str_cur_dir 为程序运行路径，一般为项目根目录，网站文件存放在resources中
    strcpy(m_real_file, std::string (g_str_cur_dir + "/resources").c_str());
    int len = strlen(std::string (g_str_cur_dir + "/resources").c_str());
    const char *p = strrchr(m_url, '/');


    if (m_ispost == 0) {

        // 访问index.html界面
        if (*(p + 1) == '4') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/index.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));  //拼接路径
            free(m_url_real);
        }

        // 访问注册界面
        else if (*(p + 1) == '0') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/register.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));  //拼接路径
            free(m_url_real);
        }

        // 访问登录界面
        else if (*(p + 1) == '1') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/log.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }

        // 访问图片界面
        else if (*(p + 1) == '5') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/picture.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }

        // 访问视频界面
        else if (*(p + 1) == '6') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/video.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }

        // 访问关注界面
        else if (*(p + 1) == '7') {
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/fans.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }
        else  { // 其他界面
            strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
        }
    }
    else if(m_ispost == 1) {

        //处理post请求 且是登录或者注册
        if ((*(p + 1) == '2' || *(p + 1) == '3')) {

            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/");
            strcat(m_url_real, m_url + 2);
            strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
            free(m_url_real);

            //将用户名和密码提取出来
            //user=abcd&password=asdasd
            char name[100], password[100];
            int i;
            for (i = 5; m_string[i] != '&'; ++i)
                name[i - 5] = m_string[i];
            name[i - 5] = '\0';

            int j = 0;        // 此时i在&这里，加10刚好到密码的第一个字符
            for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
                password[j] = m_string[i];
            password[j] = '\0';

            if (*(p + 1) == '3') {
                //注册，先检测数据库中是否有重名的
                //没有重名的，进行增加数据
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");
                if (users.find(name) == users.end())
                {
                    m_lock_map.lock();
//                int res = mysql_query(mysql, sql_insert);
                    int res = 0; // todo 这里假设总是注册成功
                    m_lock_map.unlock();

                    if (!res) {
                        m_lock_map.lock();
                        users.insert(std::pair<std::string, std::string>(name, password));
                        m_lock_map.unlock();

                        strcpy(m_url, "/log.html");
                        std::cout << "新用户注册成功：user = " << name << ", password = " << password << std::endl;
                    }
                    else {
                        strcpy(m_url, "/registerError.html");
                        std::cout << "新用户注册失败，数据库写入错误：user = " << name << ", password = " << password << std::endl;
                    }
                }
                else {
                    strcpy(m_url, "/registerError.html");
                    std::cout << "新用户注册失败，用户名重复：user = " << name << ", password = " << password << std::endl;

                }
            }
                //如果是登录，直接判断
                //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            else if (*(p + 1) == '2') {
                if (users.find(name) != users.end() && users[name] == password)
                    strcpy(m_url, "/welcome.html");
                else
                    strcpy(m_url, "/logError.html");
            }
        }
    }


    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if (stat(m_real_file, m_file_stat) < 0)
        return NO_RESOURCE;

    // 判断访问权限
    if (!((*m_file_stat).st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 判断是否是目录
    if (S_ISDIR((*m_file_stat).st_mode))
        return BAD_REQUEST;

    return FILE_REQUEST;
}



