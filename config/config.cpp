
#include "config.h"

Config::Config() {
    //端口号,默认9006
    m_PORT = 9006;

    //日志写入方式，默认同步
    m_LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    m_TriggerMode = 0;

    //优雅关闭链接，默认不使用
    m_OPT_LINGER = 0;

    //数据库连接池数量,默认8
    m_sql_num = 8;

    //线程池内的线程数量,默认8
    m_thread_num = 8;

    //关闭日志,默认不关闭
    m_close_log = 0;

    //并发模型,默认是proactor
    m_actor_model = 0;

    //需要修改的数据库信息,登录名,密码,库名
    m_sql_user = "chen";
    m_sql_passwd = "12345678";
    m_sql_databasename = "webserver";
}

int Config::loadconfig(const std::string &config_filename) {
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_filename);
    } catch (YAML::BadFile &e) {
        std::cout << "yaml file read error! " << std::endl;
        return -1;
    }
    //端口号
    m_PORT = config["PORT"].as<int>();

    //日志写入方式
    m_LOGWrite = config["LOGWrite"].as<int>();

    //触发组合模式
    m_TriggerMode = config["TriggerMode"].as<int>();

    //优雅关闭链接
    m_OPT_LINGER = config["OPT_LINGER"].as<int>();

    //数据库连接池数量
    m_sql_num = config["sql_num"].as<int>();

    //线程池内的线程数量
    m_thread_num = config["thread_num"].as<int>();

    //是否关闭日志
    m_close_log = config["close_log"].as<int>();

    //并发模型选择
    m_actor_model = config["actor_model"].as<int>();

    // 数据库相关参数
    m_sql_user = config["databaseParameter"]["user"].as<std::string>();
    m_sql_passwd = config["databaseParameter"]["passwd"].as<std::string>();
    m_sql_databasename =
        config["databaseParameter"]["databasename"].as<std::string>();

    return 0;
}