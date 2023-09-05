#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <iostream>
#include <error.h>
#include <string.h>
#include <list>
#include <string>
#include <mysql/mysql.h>
#include "../thread_sync/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool{
public:
    // 单例模式获取对象接口
    static connection_pool* GetInstance();
    
    // 获取数据库连接
    MYSQL *GetConnection();
    // 向池中释放连接
    bool ReleaseConnection(MYSQL* conn);
    // 获取当前空闲连接的个数
    int GetFreeConn();
    // 销毁连接池
    void DestroyPool();

    // 初始化：主机地址、用户名、用户密码、数据库名字、端口、最大连接数、是否打开日志
    void init(string url, string UserName, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;           // 最大连接数
    int m_CurConn;           // 当前已使用连接数
    int m_FreeConn;          // 未使用的连接数
    locker m_lock;           // 互斥锁
    list<MYSQL*> m_connList; // 数据库连接池，使用链表实现
    sem m_sem;               // 信号量

public:
    string m_url;          // 主机地址
    string m_port;         // 数据库端口号
    string m_usrName;      // 登录用户名
    string m_usrPassWord;  // 登录用户密码
    string m_dataBaseName; // 使用数据的名字
    int m_close_log;       // 日志开关


};

// 使用RAII机制自动回收资源
class connectionRAII{
public:
    // 为什么使用双指针？
    // 因为数据库连接本身就是指针类型，所以要想修改它，必须使用双指针
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;

};


#endif