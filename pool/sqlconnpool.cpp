#include "sqlconnpool.h"

SqlConnPool::SqlConnPool() {
    m_CurConn = 0;
    m_FreeConn = 0;
}

SqlConnPool::~SqlConnPool() {
    DestroyPool();
}

SqlConnPool *SqlConnPool::GetInstance() {
    static SqlConnPool connPool;
    return &connPool;
}

// 初始化
void SqlConnPool::init(std::string url, std::string User, std::string PassWord, std::string DatabaseName, int Port,
                       int MaxConn) {
    // 初始化数据库信息
    m_url = url;
    m_Port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DatabaseName;

    for (int i = 0; i < MaxConn; i++) {
        MYSQL *con = nullptr;

        con = mysql_init(con);
        if (con == nullptr) {
            LOG_ERROR("MySQL Init Error: %s", mysql_error(con));
            exit(1);
        }

        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), m_DatabaseName.c_str(), Port, nullptr, 0);
        if (con == nullptr) {
            LOG_ERROR("MySQL real_connect Error: %s", mysql_error(con));
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = Sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

// 当前空闲的连接数
int SqlConnPool::GetFreeConn() {
    return this->m_FreeConn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *SqlConnPool::GetConnection() {
    MYSQL *con = nullptr;

    if (connList.size() == 0)
        return nullptr;

    reserve.wait();  // 取出连接，信号量原子减1，为0则等待
    lock.lock();

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

// 释放当前使用连接，回收到线程池
bool SqlConnPool::ReleaseConnection(MYSQL *conn) {
    if (conn == nullptr)
        return false;

    lock.lock();

    connList.push_back(conn);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post();  //释放连接原子加1
    return true;
}



void SqlConnPool::DestroyPool() {
    lock.lock();
    if (connList.size() > 0) {
        std::list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}







