
#include "config.h"

Config::Config() {
    port_ = 10000;
    trigger_mode_ = 0;
    actor_model_ = 0;
    elegant_close_linger_ = 1;

    sql_port_ = 3306;
    sql_host_ = "localhost";
    sql_user_ = "chen";
    sql_password_ = "12345678";
    sql_thread_num_ = 8;
    sql_database_name_ = "webserver";

    log_write_way_ = 0;
    log_is_open_ = 1;
    log_level_ = 1;
    log_file_path_ = "./bin/log/chenWebLog";
}

int Config::LoadConfig(const std::string &config_filename) {
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_filename);
    } catch (YAML::BadFile &e) {
        return -1;
    }

    port_ = config["Port"].as<int>();  // 端口号
    // max_requests_ = config["MaxRequests"].as<int>();  // 任务请求队列最大数量
    trigger_mode_ = config["TriggerMode"].as<int>();  // 触发组合模式
    thread_num_ = config["ThreadNum"].as<int>();  // 线程池内的线程数量
    elegant_close_linger_ =
        config["ElegantCloseLinger"].as<int>();     // 优雅关闭链接
    actor_model_ = config["ActorModel"].as<int>();  // 并发模型选择

    // 日志相关参数
    log_is_open_ = config["Log"]["LogIsOpen"].as<int>();
    log_write_way_ = config["Log"]["LogWriteWay"].as<int>();
    log_level_ = config["Log"]["LogLevel"].as<int>();
    log_file_path_ = config["Log"]["LogFilePath"].as<std::string>();

    // 数据库相关参数
    sql_port_ = config["DatabaseParameter"]["SqlPort"].as<int>();
    sql_user_ = config["DatabaseParameter"]["User"].as<std::string>();
    sql_password_ = config["DatabaseParameter"]["PassWord"].as<std::string>();
    sql_thread_num_ = config["DatabaseParameter"]["SqlThreadNum"].as<int>();
    sql_database_name_ =
        config["DatabaseParameter"]["DatabaseName"].as<std::string>();
    sql_host_ = config["DatabaseParameter"]["SqlHost"].as<std::string>();

    return 0;
}