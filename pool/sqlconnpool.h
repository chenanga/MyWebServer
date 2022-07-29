#ifndef CHENWEB_POOL_SQLCONNPOOL_H
#define CHENWEB_POOL_SQLCONNPOOL_H

#include <mysql/mysql.h>

#include <cassert>
#include <iostream>
#include <list>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"

class SqlConnPool {
public:
    MYSQL *GetConnection();               //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);  //释放连接
    inline int GetFreeConn() {
        return this->free_conn_;
    };                   //获取当前空闲的连接数
    void DestroyPool();  //销毁所有连接

    //单例模式
    static SqlConnPool *GetInstance();

    void init(std::string url, std::string user, std::string password,
              std::string database_name, int port, int max_conn);

private:
    SqlConnPool();
    ~SqlConnPool();

    int max_conn_;   //最大连接数
    int cur_conn_;   //当前已使用的连接数
    int free_conn_;  //当前空闲的连接数
    Locker lock_;
    std::list<MYSQL *> conn_list_;  //连接池
    Sem reserve_;                   // 信号量

public:
    std::string url_;            //主机地址
    std::string port_;           //数据库端口号
    std::string user_;           //登陆数据库用户名
    std::string password_;       //登陆数据库密码
    std::string database_name_;  //使用数据库名
};

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    // 双指针对MYSQL *sql修改
    SqlConnRAII(MYSQL **sql, SqlConnPool *conn_pool) {
        *sql = conn_pool->GetConnection();
        sql_ = *sql;
        connpool_ = conn_pool;
    }

    ~SqlConnRAII() {
        if (sql_) {
            connpool_->ReleaseConnection(sql_);
        }
    }

private:
    MYSQL *sql_;
    SqlConnPool *connpool_;
};

#endif  // CHENWEB_POOL_SQLCONNPOOL_H
