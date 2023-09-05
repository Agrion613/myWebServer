#include <mysql/mysql.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <list>
#include <stdlib.h>
#include <pthread.h>
#include <string>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

// 连接池类初始化
void connection_pool::init(string url, string UserName, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log){
    m_url = url;
    m_usrName = UserName;
    m_usrPassWord = PassWord;
    m_dataBaseName = DataBaseName;
    m_port = Port;
    m_close_log = close_log;

    // 创建MaxConn个数据库连接
    for(int i = 0; i < MaxConn; i++){
        MYSQL *con = NULL;
        // MYSQL初始化
        con = mysql_init(con);

        // MYSQL初始化错误
        if(con == NULL){
            LOG_ERROR("MySQL Error!");
            exit(1);
        }

        // 创建MYSQL连接
        con = mysql_real_connect(con, url.c_str(), UserName.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);
        if(con == NULL){
            LOG_ERROR("MySQL Error!");
            exit(1);
        }
        m_connList.push_back(con);
        // 将一个创建好的MYSQL连接加入链表后，就代表当前池中的可用连接多一个
        m_FreeConn++;
    }

    // 初始化信号量，将可用连接数量作为信号量个数
    m_sem = sem(m_FreeConn);
    // 注意这里是将可用连接数量作为最大连接数，因为可能创建好的连接数不足MaxConn
    m_MaxConn = m_FreeConn;
}

// 当有新的连接请求时，从池中获取一个可用连接，更新已使用的连接数和空闲连接数
MYSQL *connection_pool::GetConnection(){
    MYSQL *con = NULL;

    // 如果连接池队列大小为0，则代表无法获取连接，直接返回NULL
    if(0 == m_connList.size()){
        return NULL;
    }

    // 当要获取连接时，使信号量减一，如果减一后小于0
    // 则阻塞在此，直到有可用信号量
    m_sem.wait();

    // 加锁为了线程间同步
    m_lock.lock();

    con = m_connList.front();
    m_connList.pop_front();
    m_FreeConn--;
    m_CurConn++;

    m_lock.unlock();
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *conn){
    if(conn == NULL){
        return false;
    }

    // 加锁
    m_lock.lock();

    m_connList.push_back(conn);
    m_CurConn--;
    m_FreeConn++;

    m_lock.unlock();

    // 释放了一个连接后，信号量就会加一
    m_sem.post();
    
    return true;
}

// 销毁连接池
void connection_pool::DestroyPool(){
    // 销毁连接池需要获取锁
    m_lock.lock();

    // 如果池中（即连接链表）连接个数大于0
    // 则需要将它们全部关闭
    if(m_connList.size() > 0){
        list<MYSQL *>::iterator it;

        // for循环逐个关闭所有连接
        for(it = m_connList.begin(); it != m_connList.end(); it++){
            MYSQL *con = *it;
            mysql_close(con);
        }

        m_CurConn = 0;
        m_FreeConn = 0;
        m_connList.clear();
    }

    m_lock.unlock();
}

// 获取当前空闲连接数
int connection_pool::GetFreeConn(){
    return this->m_FreeConn;
}

// 析构函数
connection_pool::~connection_pool(){
    DestroyPool();
}


// 不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行获取和释放。
connectionRAII::connectionRAII(MYSQL **con, connection_pool *connPool){
    
    *con = connPool->GetConnection();

    conRAII = *con;
    poolRAII = connPool;
}
connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(conRAII);
}
