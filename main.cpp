#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <csignal>
#include <cassert>
#include "pool/threadpool.h"
#include "config/config.h"
#include "http/http_conn.h"
#include "utils/utils.h"
#include "global/global.h"
#include "timer/listtimer.h"
#include "log/log.h"
#include "pool/sqlconnpool.h"

const int MAX_FD = 65536;  //最大文件描述符
const int MAX_EVENT_NUMBER = 20000; //最大事件数
const int TIMESLOT = 5;



int main(int argc, char * argv[]) {
    // todo 根据config判断是异步还是同步
    Log::get_instance()->init("./bin/log/chenWebLog", 2000, 800000, 0, 1);

    LOG_INFO("========== Server start ==========");

    int ret = 0;
//    if (argc <= 1) {
//        std::cout << "按照如下格式运行：" << basename(argv[0]) << "port_number\n" << std::endl;
//        exit(-1);
//    }

    // 获取当前工作路径
    char* cur_dir;
    if ((cur_dir = getcwd(NULL, 0)) == NULL) {
        perror("getcwd error");
        exit(-1);
    }
    g_str_cur_dir = cur_dir;


    // 配置文件解析
    std::string config_file_path = g_str_cur_dir + "/config.yaml";
    Config config;
    config.loadconfig(config_file_path);

    if(config.m_PORT > 65535 || config.m_PORT < 1024) {
        LOG_ERROR("Port:%d error!",  config.m_PORT);
        exit(-1);
    }
    std::cout << "浏览器端按照 ip:" << config.m_PORT << " 方式运行，例如 http://192.168.184.132:10000/" << std::endl;


    //创建数据库连接池
    SqlConnPool *sql_conn_pool = SqlConnPool::GetInstance();
    sql_conn_pool->init("localhost", "chen", "12345678", "webserver", 3306, 8);
    initmysql_result(sql_conn_pool);

    // 创建线程池，初始化
    ThreadPool<HttpConn> * pool = nullptr;
    try{
        pool = new ThreadPool<HttpConn>(sql_conn_pool);
    }
    catch(...) {
        LOG_ERROR("线程池初始化失败");
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    HttpConn * users = new HttpConn[MAX_FD];

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用, 需要绑定之前设置
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.m_PORT);
    ret = bind(listen_fd, (struct sockaddr*)&address, sizeof(address));
    if (ret == -1) {
        perror("bind");
        exit(-1);
    }

    // 监听
    ret = listen(listen_fd, 5);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }


    // 创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epoll_fd = epoll_create(5);

    // 将监听的文件描述符添加到epoll对象中
    addFd(epoll_fd, listen_fd, false, config.m_TriggerMode);
    HttpConn::m_epoll_fd = epoll_fd;



    // 定时器相关
    int pipefd[2];  // 定时器用管道
    static ListTimer timer_list;      //创建定时器容器链表
    client_data *users_timer = new client_data[MAX_FD];      //创建连接资源数组
    bool timeout = false;  //超时默认为False
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    TimerUtils timer_utils;
    timer_utils.init(TIMESLOT);
    timer_utils.setnonblocking(pipefd[1]);
    timer_utils.addfd(epoll_fd, pipefd[0], false, 0);
    timer_utils.addsig(SIGPIPE, SIG_IGN);      // 处理 SIGPIPE 信号

    timer_utils.addsig(SIGALRM, timer_utils.sig_handler, false);
    timer_utils.addsig(SIGTERM, timer_utils.sig_handler, false);
    alarm(TIMESLOT);  //alarm定时触发SIGALRM信号
    TimerUtils::u_pipefd = pipefd;
    TimerUtils::u_epollfd = epoll_fd;
    bool stop_server = false;


    while (!stop_server) {
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR) {
            // 调用失败
            // todo log(::)
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; ++ i) {
            int sock_fd = events[i].data.fd;
            if (sock_fd == listen_fd) {
                // 有客户端连接, 初始化客户端连接地址
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                int conn_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_address_len);
                if ( conn_fd < 0 ) {
                    LOG_WARN("errno is: %d", errno);
                    continue;
                }
                if (HttpConn::m_user_count >= MAX_FD) {
                    // 目前连接数满了，给客户端写一个信息，服务器内部正忙
                    // send message   todo
                    close(conn_fd);
                    continue;
                }
                LOG_INFO("Client connect : conn_fd = %d, client_address = %s", conn_fd, inet_ntoa(client_address.sin_addr));
                // 将新的客户数据初始化，放入数组
                users[conn_fd].init(conn_fd, client_address, config.m_TriggerMode);

                // 定时器处理
                users_timer[conn_fd].address = client_address;
                users_timer[conn_fd].sock_fd = conn_fd;

                Timer *timer = new Timer;                   //创建定时器临时变量
                timer->user_data = &users_timer[conn_fd];            //设置定时器对应的连接资源
                timer->cb_func = cb_func;                 //设置回调函数

                time_t cur = time(nullptr);
                //设置绝对超时时间, 为3倍的TIMESLOT
                timer->expire = cur + 3 * TIMESLOT;
                users_timer[conn_fd].timer = timer;                //创建该连接对应的定时器，初始化为前述临时变量
                timer_utils.m_timer_list.add_timer(timer);                //将该定时器添加到链表中
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {

                //服务器端关闭连接，移除对应的定时器
                Timer *timer = users_timer[sock_fd].timer;
                timer->cb_func(&users_timer[sock_fd]);
                if (timer) {
                    timer_utils.m_timer_list.del_timer(timer);
                }
            }

            // 处理定时器信号
            else if( ( sock_fd == pipefd[0] ) && ( events[i].events & EPOLLIN ) ) {

                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                    continue;

                else {
                    for (int time_i = 0; time_i < ret; ++ time_i) {
                        switch (signals[time_i]) {
                            case SIGALRM: {
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                                stop_server = true;  // 主动关闭进程
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {  // 读事件
                Timer *timer = users_timer[sock_fd].timer;
                if (users[sock_fd].read()) {
                    // 一次性读取完所有数据
                    pool->append(users + sock_fd);  // 交给工作线程
                    if (timer) {  // 更新定时器
                        time_t cur = time(nullptr);
                        timer->expire = cur + 3 * TIMESLOT;
                        timer_utils.m_timer_list.adjust_timer(timer);
                    }
                }
                else  {
                    //服务器端关闭连接，移除对应的定时器
                    timer->cb_func(&users_timer[sock_fd]);
                    if (timer)
                        timer_utils.m_timer_list.del_timer(timer);
                }

            }
            else if (events[i].events & EPOLLOUT) {  // 写事件
                Timer *timer = users_timer[sock_fd].timer;
                if (users[sock_fd].write()) {  // 一次性写完所有数据
                    if (timer) {  // 更新定时器
                        time_t cur = time(nullptr);
                        timer->expire = cur + 3 * TIMESLOT;
                        timer_utils.m_timer_list.adjust_timer(timer);
                    }
                }
                else {
                    //服务器端关闭连接，移除对应的定时器
                    timer->cb_func(&users_timer[sock_fd]);
                    if (timer)
                        timer_utils.m_timer_list.del_timer(timer);
                }
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if (timeout) {
            timer_utils.timer_handler();
            timeout = false;
        }
    }
    close(epoll_fd);
    close(listen_fd);
    delete[] users;
    delete pool;
    free(cur_dir);
    return 0;
}
