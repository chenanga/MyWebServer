
#include "webserver.h"

WebServer::WebServer(const std::string &config_file_name) {
    // 获取当前工作路径
    char *cur_dir;
    if ((cur_dir = getcwd(nullptr, 0)) == nullptr) {
        perror("getcwd error");
        exit(-1);
    }
    kStrCurDir = cur_dir;

    // 配置文件解析
    std::string config_file_path = kStrCurDir + "/" + config_file_name;
    int ret = config_.LoadConfig(config_file_path);
    if (ret == -1) {
        std::cout << "yaml file Read error! " << std::endl;
        exit(-1);
    }

    users_ = new HttpConn[MAX_FD];          // 客户端信息
    users_timer_ = new ClientData[MAX_FD];  //创建连接资源数组
    free(cur_dir);
    Init();
}

WebServer::~WebServer() {
    close(epoll_fd_);
    close(listen_fd_);
    delete[] users_;
    delete thread_pool_;
    delete users_timer_;
}

void WebServer::Init() {
    port_ = config_.port_;
    trigger_mode_ = config_.trigger_mode_;
    actor_model_ = config_.actor_model_;
    elegant_close_linger_ = config_.elegant_close_linger_;
    sql_port_ = config_.sql_port_;
    sql_host_ = config_.sql_host_;
    sql_user_ = config_.sql_user_;
    sql_password_ = config_.sql_password_;
    sql_thread_num_ = config_.sql_thread_num_;
    sql_database_name_ = config_.sql_database_name_;

    log_write_way_ = config_.log_write_way_;
    log_is_open_ = config_.log_is_open_;
    log_level_ = config_.log_level_;
    log_file_path_ = config_.log_file_path_;

    LogInit();
    CheckPort();
    SqlInit();
    SetTriggerMode();
    EventListen();
}

void WebServer::CheckPort() const {
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!", port_);
        exit(-1);
    }
}

void WebServer::SqlInit() {
    sql_conn_pool_ = SqlConnPool::GetInstance();
    sql_conn_pool_->init(sql_host_, sql_user_, sql_password_,
                         sql_database_name_, sql_port_, sql_thread_num_);
    InitMysqlResult(sql_conn_pool_);
}

void WebServer::LogInit() {
    if (log_is_open_) {
        if (log_write_way_ == 0)
            Log::GetInstance()->Init(log_file_path_.c_str(), 2000, 800000, 0,
                                     log_level_);
        else
            Log::GetInstance()->Init(log_file_path_.c_str(), 2000, 800000, 1024,
                                     log_level_);
    }
    LOG_INFO("========== Server init ==========");
}

void WebServer::SetTriggerMode() {
    switch (trigger_mode_) {
        case 0:
            listen_fd_trigger_mode_ = 0;
            client_fd_trigger_mode_ = 0;
            break;
        case 1:
            listen_fd_trigger_mode_ = 0;
            client_fd_trigger_mode_ = 1;
            break;
        case 2:
            listen_fd_trigger_mode_ = 1;
            client_fd_trigger_mode_ = 0;
            break;
        case 3:
            listen_fd_trigger_mode_ = 1;
            client_fd_trigger_mode_ = 1;
            break;
        default:
            listen_fd_trigger_mode_ = 1;
            client_fd_trigger_mode_ = 1;
    }
}

void WebServer::EventListen() {
    try {
        thread_pool_ = new ThreadPool<HttpConn>(sql_conn_pool_);
    } catch (...) {
        LOG_ERROR("thread_pool_ init failure !");
        exit(-1);
    }

    listen_fd_ = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        LOG_ERROR("socket failure: %s", error);
        exit(-1);
    }

    // 优雅关闭: 直到所剩数据发送完毕或超时
    if (elegant_close_linger_ == 1) {
        struct linger linger_ = {0, 0};
        setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &linger_,
                   sizeof(linger_));
    } else {
        struct linger linger_ = {1, 1};
        setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &linger_,
                   sizeof(linger_));
    }

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config_.port_);
    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int ret = bind(listen_fd_, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1) {
        LOG_ERROR("bindfailure: %s", error);
        exit(-1);
    }

    // 监听
    ret = listen(listen_fd_, 6);
    if (ret == -1) {
        LOG_ERROR("listen failure: %s", error);
        exit(-1);
    }

    // 创建epoll对象
    epoll_fd_ = epoll_create(5);
    if (epoll_fd_ == -1) {
        LOG_ERROR("epoll_create failure: %s", error);
        exit(-1);
    }
    // 将监听的文件描述符添加到epoll对象中
    AddFd(epoll_fd_, listen_fd_, false, listen_fd_trigger_mode_);
    HttpConn::epoll_fd_ = epoll_fd_;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd_);
    if (ret == -1) {
        LOG_ERROR("socketpair failure: %s", error);
        exit(-1);
    }

    timer_utils_.Init(TIMESLOT);
    timer_utils_.SetNonBlocking(pipe_fd_[1]);
    timer_utils_.AddFd(epoll_fd_, pipe_fd_[0], false, 0);
    timer_utils_.AddSignal(SIGPIPE, SIG_IGN);  // 处理 SIGPIPE 信号
    timer_utils_.AddSignal(SIGALRM, TimerUtils::SignalHandler, false);
    timer_utils_.AddSignal(SIGTERM, TimerUtils::SignalHandler, false);
    TimerUtils::pipe_fd_ = pipe_fd_;
    TimerUtils::epoll_fd_ = epoll_fd_;
    alarm(TIMESLOT);  // alarm定时触发SIGALRM信号
}

void WebServer::Start() {
    LOG_INFO("========== Server start ==========");
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server) {
        int num = epoll_wait(epoll_fd_, events_, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR) {
            LOG_ERROR("epoll_wait failure: %s", error);
            break;
        }

        for (int i = 0; i < num; ++i) {
            int sock_fd = events_[i].data.fd;
            if (sock_fd == listen_fd_) {
                bool ret = DealClientConn();
                if (!ret) continue;
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //服务器端关闭连接，移除对应的定时器
                Timer *timer = users_timer_[sock_fd].timer;
                CloseTimer(timer, sock_fd);
            } else if ((sock_fd == pipe_fd_[0]) &&
                       (events_[i].events & EPOLLIN)) {
                bool ret = DealSignal(timeout, stop_server);
                if (!ret) LOG_ERROR("%s", "Deal Client data failure");
            } else if (events_[i].events & EPOLLIN)
                DealReadEvent(sock_fd);
            else if (events_[i].events & EPOLLOUT)
                DealWriteEvent(sock_fd);
        }
        if (timeout) {
            timer_utils_.TimerHandler();
            timeout = false;
        }
    }
}

bool WebServer::DealClientConn() {
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    if (listen_fd_trigger_mode_ == 0) {
        int conn_fd = accept(listen_fd_, (struct sockaddr *)&client_address,
                             &client_address_len);
        if (conn_fd < 0) {
            LOG_WARN("errno is: %d", errno);
            return false;
        }
        if (HttpConn::user_count_ >= MAX_FD) {
            SendError(conn_fd, "Server busy!");
            return false;
        }
        SetTimer(conn_fd, client_address);
    } else {
        while (1) {
            int conn_fd = accept(listen_fd_, (struct sockaddr *)&client_address,
                                 &client_address_len);
            if (conn_fd < 0) {
                LOG_WARN("errno is: %d", errno);
                break;
            }
            if (HttpConn::user_count_ >= MAX_FD) {
                SendError(conn_fd, "Server busy!");
                break;
            }
            SetTimer(conn_fd, client_address);
        }
        return false;
    }
    return true;
}

void WebServer::SendError(int fd, const char *info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::SetTimer(int conn_fd, struct sockaddr_in client_address) {
    users_[conn_fd].Init(conn_fd, client_address, client_fd_trigger_mode_);
    // 定时器处理
    users_timer_[conn_fd].address = client_address;
    users_timer_[conn_fd].sock_fd = conn_fd;

    Timer *timer = new Timer;  //创建定时器临时变量
    //设置定时器对应的连接资源
    timer->user_data_ = &users_timer_[conn_fd];
    timer->CallbackFunction = CallbackFunction;  //设置回调函数

    time_t cur = time(nullptr);
    //设置绝对超时时间, 为3倍的TIMESLOT
    timer->expire_ = cur + 3 * TIMESLOT;
    users_timer_[conn_fd].timer =
        timer;  //创建该连接对应的定时器，初始化为前述临时变量
    timer_utils_.timer_list_.AddTimer(timer);  //将该定时器添加到链表中
}

void WebServer::CloseTimer(Timer *timer, int sock_fd) {
    timer->CallbackFunction(&users_timer_[sock_fd]);
    if (timer) timer_utils_.timer_list_.DelTimer(timer);
    LOG_INFO("close fd %d", users_timer_[sock_fd].sock_fd);
}

bool WebServer::DealSignal(bool &timeout, bool &stop_server) {
    int ret;
    char signals[1024];
    ret = recv(pipe_fd_[0], signals, sizeof(signals), 0);
    if (ret == -1 || ret == 0)
        return false;
    else {
        for (int i = 0; i < ret; ++i) {
            switch (signals[i]) {
                case SIGALRM: {
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;  // 主动关闭进程
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::DealReadEvent(int sock_fd) {
    Timer *timer = users_timer_[sock_fd].timer;
    if (actor_model_ == 0) {
        if (users_[sock_fd].Read()) {
            // 一次性读取完所有数据
            LOG_INFO("Deal with the client(%s)",
                     inet_ntoa(users_[sock_fd].getAddress()->sin_addr));
            thread_pool_->Append(users_ + sock_fd);  // 交给工作线程
            if (timer)                               // 更新定时器
                AdjustTimer(timer);
        } else
            CloseTimer(timer, sock_fd);

    } else {
        // todo
    }
}

void WebServer::DealWriteEvent(int sock_fd) {
    Timer *timer = users_timer_[sock_fd].timer;
    if (actor_model_ == 0) {
        if (users_[sock_fd].Write()) {  // 一次性写完所有数据
            LOG_INFO("Send data to the client(%s)",
                     inet_ntoa(users_[sock_fd].getAddress()->sin_addr));
            if (timer)  // 更新定时器
                AdjustTimer(timer);
        } else
            CloseTimer(timer, sock_fd);
    } else {
        // todo
    }
}

void WebServer::AdjustTimer(Timer *timer) {
    time_t cur = time(nullptr);
    timer->expire_ = cur + 3 * TIMESLOT;
    timer_utils_.timer_list_.AdjustTimer(timer);
    LOG_INFO("client address : %s, adjust timer once",
             inet_ntoa(timer->user_data_->address.sin_addr));
}
