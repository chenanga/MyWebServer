

#ifndef CHENWEB_SQLCONNPOOL_H
#define CHENWEB_SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <list>
#include <cassert>

#include "../log/log.h"
#include "../lock/locker.h"


class SqlConnPool
{
public:
    MYSQL *GetConnection();				 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn); //释放连接
    int GetFreeConn();					 //获取连接
    void DestroyPool();					 //销毁所有连接

    //单例模式
    static SqlConnPool *GetInstance();

    void init(std::string url, std::string User, std::string PassWord, std::string DatabaseName, int Port, int MaxConn);

private:
    SqlConnPool();
    ~SqlConnPool();

    int m_MaxConn;  //最大连接数
    int m_CurConn;  //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数
    Locker lock;
    std::list<MYSQL *> connList; //连接池
    Sem reserve;  // 信号量

public:
    std::string m_url;			 //主机地址
    std::string m_Port;		 //数据库端口号
    std::string m_User;		 //登陆数据库用户名
    std::string m_PassWord;	 //登陆数据库密码
    std::string m_DatabaseName; //使用数据库名
};

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    // 双指针对MYSQL *sql修改
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        *sql = connpool->GetConnection();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~SqlConnRAII() {
        if(sql_) { connpool_->ReleaseConnection(sql_); }
    }

private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //CHENWEB_SQLCONNPOOL_H
