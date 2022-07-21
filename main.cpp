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
    std::string str_cur_dir = cur_dir;


    // 配置文件解析
    std::string config_file_path = str_cur_dir + "/config.yaml";
    Config config;
    config.loadconfig(config_file_path);

    // 处理 SIGPIPE 信号
    addSig(SIGPIPE, SIG_IGN);

    // 创建线程池，初始化
    Thread_pool<Http_conn> * pool = nullptr;
    try{
        pool = new Thread_pool<Http_conn>();
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
    Http_conn::m_epoll_fd = epoll_fd;

    while (true) {
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
                // 有客户端连接
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                int conn_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_address_len);
                if (Http_conn::m_user_count >= MAX_FD) {
                    // 目前连接数满了，给客户端写一个信息，服务器内部正忙
                    // send message   todo
                    close(conn_fd);
                    continue;
                }

                // 将新的客户数据初始化，放入数组
                users[conn_fd].init(conn_fd, client_address, config.m_TriggerMode);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开或者错误等事件
                users[sock_fd].close_conn();
            }
            else if (events[i].events & EPOLLIN) {  // 读事件
                //
                if (users[sock_fd].read()) {
                    // 一次性读取完所以数据
                    pool->append(users + sock_fd);  // 交给工作线程
                }
                else  // 读取失败
                    users[sock_fd].close_conn();

            }
            else if (events[i].events & EPOLLOUT) {  // 写事件
                if (!users[sock_fd].write()) {  // 一次性写完所有数据
                    // 失败了，关闭连接
                    users[sock_fd].close_conn();
                }
            }
        }
    }
    close(epoll_fd);
    close(listen_fd);
    delete[] users;
    delete pool;
    free(cur_dir);
    return 0;
}
