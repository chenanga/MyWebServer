
#ifndef MYWEB_CONFIG_H
#define MYWEB_CONFIG_H

#include <iostream>
#include <fstream>
#include <string>
#include "yaml-cpp/yaml.h"
#include "../log/log.h"
class Config
{
public:
    Config();
    ~Config(){};

    int loadconfig(const std::string &config_filename);

    //端口号
    int m_PORT;

    //日志写入方式
    int m_LOGWrite;

    //触发组合模式
    int m_TriggerMode;

    //优雅关闭链接
    int m_OPT_LINGER;

    //数据库连接池数量
    int m_sql_num;

    //线程池内的线程数量
    int m_thread_num;

    //是否关闭日志
    int m_close_log;

    //并发模型选择
    int m_actor_model;

    //需要修改的数据库信息,登录名,密码,库名
    std::string m_sql_user;
    std::string m_sql_passwd;
    std::string m_sql_databasename;
};

#endif //MYWEB_CONFIG_H
