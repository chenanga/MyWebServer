
#ifndef MYWEB_CONFIG_H
#define MYWEB_CONFIG_H

#include <fstream>
#include <iostream>
#include <string>

#include "../log/log.h"
#include "yaml-cpp/yaml.h"

class Config {
public:
    Config();
    ~Config(){};

    int loadconfig(const std::string &config_filename);

    int m_PORT;         //端口号
    int m_LOGWrite;     //日志写入方式
    int m_TriggerMode;  //触发组合模式
    int m_OPT_LINGER;   //优雅关闭链接
    int m_sql_num;      //数据库连接池数量
    int m_thread_num;   //线程池内的线程数量
    int m_close_log;    //是否关闭日志
    int m_actor_model;  //并发模型选择

    //需要修改的数据库信息
    std::string m_sql_user;          //登录名
    std::string m_sql_passwd;        //密码
    std::string m_sql_databasename;  //数据库库名
};

#endif  // MYWEB_CONFIG_H
