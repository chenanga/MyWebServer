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


const int MAX_FD = 65536;  //最大文件描述符
const int MAX_EVENT_NUMBER = 20000; //最大事件数



int main(int argc, char * argv[]) {

    if (argc <= 1) {
        std::cout << "按照如下格式运行：" << basename(argv[0]) << "port_number\n" << std::endl;
        exit(-1);
    }

    // 配置文件解析
    std::string config_file_path = "./config.yaml";
    Config config;
    config.loadconfig(config_file_path);

    // 处理 SIGPIPE 信号
    addSig(SIGPIPE, SIG_IGN);

    // 创建线程池，初始化
    Thread_pool<Http_conn> * pool = nullptr;
    try{
        pool = new Thread_pool<Http_conn>;
    }
    catch(...) {
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    Http_conn * users = new Http_conn[MAX_FD];

    //
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用, 需要绑定之前设置
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    int ret = 0;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.m_PORT);
    ret = bind(listen_fd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    // 监听
    ret = listen(listen_fd, 5);
    assert(ret >= 0);

    // 创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epoll_fd = epoll_create(5);

    // 将监听的文件描述符添加到epoll对象中
    addFd(epoll_fd, listen_fd, false, config.m_TriggerMode);
    Http_conn::m_epoll_fd = epoll_fd;

    while (true) {
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR) {
            // 调用失败
            // todo log(::)
            break;
        }

        // 循环遍历事件数组
    }

    return 0;
}
