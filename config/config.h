#ifndef CHENWEB_CONFIG_CONFIG_H
#define CHENWEB_CONFIG_CONFIG_H

#include <fstream>
#include <iostream>
#include <string>

#include "../log/log.h"
#include "yaml-cpp/yaml.h"

class Config {
public:
    Config();
    ~Config(){};

    int LoadConfig(const std::string &config_filename);

    int port_;                  // 端口号
    int thread_num_;            // 线程池内的线程数量
    int max_requests_;          // 任务请求队列最大数量
    int actor_model_;           // 并发模型选择
    int trigger_mode_;          // 触发组合模式
    int elegant_close_linger_;  // 优雅关闭链接

    int log_write_way_;          // 日志写入方式
    int log_is_open_;            // 日志开关
    int log_level_;              // 日志等级
    std::string log_file_path_;  // 日志文件路径

    // 数据库信息
    int sql_port_;                   // 数据库端口号
    int sql_thread_num_;             // 数据库连接池数量
    std::string sql_host_;           // 数据库地址
    std::string sql_user_;           // 登录名
    std::string sql_password_;       // 密码
    std::string sql_database_name_;  // 数据库库名
};

#endif  // CHENWEB_CONFIG_CONFIG_H
