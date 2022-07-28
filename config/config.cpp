
#include "config.h"

Config::Config() {
    port_ = 9006;        // 端口号,默认9006
    log_write_way_ = 0;  // 日志写入方式，默认同步
    trigger_mode_ = 0;   // 触发组合模式,默认listenfd LT + connfd LT
    elegant_close_linger_ = 0;  // 优雅关闭链接，默认不使用
    sql_num_ = 8;               // 数据库连接池数量,默认8
    thread_num_ = 8;            // 线程池内的线程数量,默认8
    close_log_ = 0;             // 关闭日志,默认不关闭
    actor_model_ = 0;           // 并发模型,默认是proactor

    //需要修改的数据库信息,登录名,密码,库名
    sql_user_ = "chen";
    sql_passwd_ = "12345678";
    sql_database_name_ = "webserver";
}

int Config::LoadConfig(const std::string &config_filename) {
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_filename);
    } catch (YAML::BadFile &e) {
        return -1;
    }

    port_ = config["PORT"].as<int>();                 // 端口号
    log_write_way_ = config["LOGWrite"].as<int>();    // 日志写入方式
    trigger_mode_ = config["TriggerMode"].as<int>();  // 触发组合模式
    elegant_close_linger_ = config["OPT_LINGER"].as<int>();  // 优雅关闭链接
    sql_num_ = config["sql_num"].as<int>();        // 数据库连接池数量
    thread_num_ = config["thread_num"].as<int>();  // 线程池内的线程数量
    close_log_ = config["close_log"].as<int>();    // 是否关闭日志
    actor_model_ = config["actor_model"].as<int>();  // 并发模型选择

    // 数据库相关参数
    sql_user_ = config["databaseParameter"]["user"].as<std::string>();
    sql_passwd_ = config["databaseParameter"]["passwd"].as<std::string>();
    sql_database_name_ =
        config["databaseParameter"]["databasename"].as<std::string>();

    return 0;
}