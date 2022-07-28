#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "config/config.h"
#include "global/global.h"
#include "http/http_conn.h"
#include "log/log.h"
#include "pool/sqlconnpool.h"
#include "pool/threadpool.h"
#include "timer/listtimer.h"
#include "utils/utils.h"

const int MAX_FD = 65536;            //最大文件描述符
const int MAX_EVENT_NUMBER = 20000;  //最大事件数
const int TIMESLOT = 5;

int main(int argc, char *argv[]) {
    // todo 根据config判断是异步还是同步
    Log::GetInstance()->Init("./bin/log/chenWebLog", 2000, 800000, 0, 1);

    LOG_INFO("========== Server start ==========");

    int ret = 0;
    //    if (argc <= 1) {
    //        std::cout << "按照如下格式运行：" << basename(argv[0]) <<
    //        "port_number\n" << std::endl; exit(-1);
    //    }

    // 获取当前工作路径
    char *cur_dir;
    if ((cur_dir = getcwd(nullptr, 0)) == nullptr) {
        perror("getcwd error");
        exit(-1);
    }
    kStrCurDir = cur_dir;

    // 配置文件解析
    std::string config_file_path = kStrCurDir + "/config.yaml";
    Config config;
    ret = config.LoadConfig(config_file_path);
    if (ret == -1) {
        LOG_ERROR("yaml file Read error! ");
        exit(-1);
    }

    if (config.port_ > 65535 || config.port_ < 1024) {
        LOG_ERROR("Port:%d error!", config.port_);
        exit(-1);
    }
    std::cout << "浏览器端按照 ip:" << config.port_
              << " 方式运行，例如 http://192.168.184.132:10000/" << std::endl;

    //创建数据库连接池
    SqlConnPool *sql_conn_pool = SqlConnPool::GetInstance();
    sql_conn_pool->init("localhost", "chen", "12345678", "webserver", 3306, 8);
    InitMysqlResult(sql_conn_pool);

    // 创建线程池，初始化
    ThreadPool<HttpConn> *pool = nullptr;
    try {
        pool = new ThreadPool<HttpConn>(sql_conn_pool);
    } catch (...) {
        LOG_ERROR("线程池初始化失败");
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    HttpConn *users = new HttpConn[MAX_FD];

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用, 需要绑定之前设置
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port_);
    ret = bind(listen_fd, (struct sockaddr *)&address, sizeof(address));
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
    AddFd(epoll_fd, listen_fd, false, config.trigger_mode_);
    HttpConn::epoll_fd_ = epoll_fd;

    // 定时器相关
    int pipefd[2];                // 定时器用管道
    static ListTimer timer_list;  //创建定时器容器链表
    ClientData *users_timer = new ClientData[MAX_FD];  //创建连接资源数组
    bool timeout = false;                                //超时默认为False
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    TimerUtils timer_utils;
    timer_utils.Init(TIMESLOT);
    timer_utils.SetNonBlocking(pipefd[1]);
    timer_utils.AddFd(epoll_fd, pipefd[0], false, 0);
    timer_utils.AddSignal(SIGPIPE, SIG_IGN);  // 处理 SIGPIPE 信号

    timer_utils.AddSignal(SIGALRM, timer_utils.SignalHandler, false);
    timer_utils.AddSignal(SIGTERM, timer_utils.SignalHandler, false);
    alarm(TIMESLOT);  // alarm定时触发SIGALRM信号
    TimerUtils::pipe_fd_ = pipefd;
    TimerUtils::epoll_fd_ = epoll_fd;
    bool stop_server = false;

    while (!stop_server) {
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR) {
            // 调用失败
            // todo log(::)
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; ++i) {
            int sock_fd = events[i].data.fd;
            if (sock_fd == listen_fd) {
                // 有客户端连接, 初始化客户端连接地址
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                int conn_fd =
                    accept(listen_fd, (struct sockaddr *)&client_address,
                           &client_address_len);
                if (conn_fd < 0) {
                    LOG_WARN("errno is: %d", errno);
                    continue;
                }
                if (HttpConn::user_count_ >= MAX_FD) {
                    // 目前连接数满了，给客户端写一个信息，服务器内部正忙
                    // send message   todo
                    close(conn_fd);
                    continue;
                }
                LOG_INFO("Client connect : conn_fd = %d, client_address = %s",
                         conn_fd, inet_ntoa(client_address.sin_addr));
                // 将新的客户数据初始化，放入数组
                users[conn_fd].Init(conn_fd, client_address,
                                    config.trigger_mode_);

                // 定时器处理
                users_timer[conn_fd].address = client_address;
                users_timer[conn_fd].sock_fd = conn_fd;

                Timer *timer = new Timer;  //创建定时器临时变量
                timer->user_data_ =
                    &users_timer[conn_fd];  //设置定时器对应的连接资源
                timer->CallbackFunction = CallbackFunction;  //设置回调函数

                time_t cur = time(nullptr);
                //设置绝对超时时间, 为3倍的TIMESLOT
                timer->expire_ = cur + 3 * TIMESLOT;
                users_timer[conn_fd].timer =
                    timer;  //创建该连接对应的定时器，初始化为前述临时变量
                timer_utils.timer_list_.AddTimer(
                        timer);  //将该定时器添加到链表中
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //服务器端关闭连接，移除对应的定时器
                Timer *timer = users_timer[sock_fd].timer;
                timer->CallbackFunction(&users_timer[sock_fd]);
                if (timer) {
                    timer_utils.timer_list_.DelTimer(timer);
                }
            }

            // 处理定时器信号
            else if ((sock_fd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                    continue;

                else {
                    for (int time_i = 0; time_i < ret; ++time_i) {
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
            } else if (events[i].events & EPOLLIN) {  // 读事件
                Timer *timer = users_timer[sock_fd].timer;
                if (users[sock_fd].Read()) {
                    // 一次性读取完所有数据
                    pool->Append(users + sock_fd);  // 交给工作线程
                    if (timer) {                    // 更新定时器
                        time_t cur = time(nullptr);
                        timer->expire_ = cur + 3 * TIMESLOT;
                        timer_utils.timer_list_.AdjustTimer(timer);
                    }
                } else {
                    //服务器端关闭连接，移除对应的定时器
                    timer->CallbackFunction(&users_timer[sock_fd]);
                    if (timer) timer_utils.timer_list_.DelTimer(timer);
                }

            } else if (events[i].events & EPOLLOUT) {  // 写事件
                Timer *timer = users_timer[sock_fd].timer;
                if (users[sock_fd].Write()) {  // 一次性写完所有数据
                    if (timer) {               // 更新定时器
                        time_t cur = time(nullptr);
                        timer->expire_ = cur + 3 * TIMESLOT;
                        timer_utils.timer_list_.AdjustTimer(timer);
                    }
                } else {
                    //服务器端关闭连接，移除对应的定时器
                    timer->CallbackFunction(&users_timer[sock_fd]);
                    if (timer) timer_utils.timer_list_.DelTimer(timer);
                }
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if (timeout) {
            timer_utils.TimerHandler();
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
