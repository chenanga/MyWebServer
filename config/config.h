
#ifndef CHENWEB_CONFIG_H
#define CHENWEB_CONFIG_H

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
    int log_write_way_;         // 日志写入方式
    int trigger_mode_;          // 触发组合模式
    int elegant_close_linger_;  // 优雅关闭链接
    int sql_num_;               // 数据库连接池数量
    int thread_num_;            // 线程池内的线程数量
    int close_log_;             // 是否关闭日志
    int actor_model_;           // 并发模型选择

    //需要修改的数据库信息
    std::string sql_user_;           // 登录名
    std::string sql_passwd_;         // 密码
    std::string sql_database_name_;  // 数据库库名
};

#endif  // MYWEB_CONFIG_H
