#include "sqlconnpool.h"

SqlConnPool::SqlConnPool() {
    cur_conn_ = 0;
    free_conn_ = 0;
}

SqlConnPool::~SqlConnPool() { DestroyPool(); }

SqlConnPool *SqlConnPool::GetInstance() {
    static SqlConnPool connPool;
    return &connPool;
}

// 初始化
void SqlConnPool::init(std::string url, std::string user, std::string password,
                       std::string database_name, int port, int max_conn) {
    // 初始化数据库信息
    url_ = url;
    port_ = port;
    user_ = user;
    password_ = password;
    database_name_ = database_name;

    for (int i = 0; i < max_conn; i++) {
        MYSQL *con = nullptr;
        con = mysql_init(con);
        if (con == nullptr) {
            LOG_ERROR("MySQL Init Error: %s", mysql_error(con));
            exit(1);
        }

        con =
            mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),
                               database_name_.c_str(), port, nullptr, 0);
        if (con == nullptr) {
            LOG_ERROR("MySQL real_connect Error: %s", mysql_error(con));
            exit(1);
        }
        conn_list_.push_back(con);
        ++free_conn_;
    }

    reserve_ = Sem(free_conn_);
    max_conn_ = free_conn_;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *SqlConnPool::GetConnection() {
    MYSQL *con = nullptr;

    if (conn_list_.size() == 0) return nullptr;
    reserve_.wait();  // 取出连接，信号量原子减1，为0则等待
    lock_.lock();

    con = conn_list_.front();
    conn_list_.pop_front();
    --free_conn_;
    ++cur_conn_;

    lock_.unlock();
    return con;
}

// 释放当前使用连接，回收到线程池
bool SqlConnPool::ReleaseConnection(MYSQL *conn) {
    if (conn == nullptr) return false;

    lock_.lock();

    conn_list_.push_back(conn);
    ++free_conn_;
    --cur_conn_;

    lock_.unlock();
    reserve_.post();  //释放连接原子加1
    return true;
}

void SqlConnPool::DestroyPool() {
    lock_.lock();
    if (conn_list_.size() > 0) {
        std::list<MYSQL *>::iterator it;
        for (it = conn_list_.begin(); it != conn_list_.end(); ++it) {
            MYSQL *con = *it;
            mysql_close(con);
        }
        cur_conn_ = 0;
        free_conn_ = 0;
        conn_list_.clear();
    }
    lock_.unlock();
}
