#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "./block_queue.h"

using namespace std;

class Log{
public:
    // 外部获取实例的方法
    // C++11之后，使用**局部变量懒汉模式**不用加锁
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }

    // 初始化函数（日志文件、日志关闭标志位、日志缓冲区大小(默认8192字节)、最大行数(默认5000000)以及最长日志数量队列）
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 写函数（写入日志的级别：DEBUG，INFO，WARN和ERROR，写入的格式化内容）
    void write_log(int level, const char *format, ...);

    // 刷新缓冲区
    void flush(void);

    // 异步写入的方法，实际流程：将日志内容放入阻塞队列后，创建一个写线程将日志写入日志文件中
    // 而这个线程的工作函数如下，其内部调用类内的一个私有成员函数async_write_log()
    static void *flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }

private:
    // 单例模式下，构造和析构函数是私有成员
    Log();
    ~Log();

    void *async_write_log(){
        string single_log;
        // 不断从阻塞队列中查询是否能获取日志，如果一直能获取到，则不断将队列中的日志写入文件
        // 如果获取不到，该线程结束
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天
    FILE *m_fp;         //打开log的文件指针
    char *m_buf;        //日志缓冲区
    block_queue<string> *m_log_queue; //阻塞队列
    bool m_is_async;                  //是否同步标志位
    locker m_mutex;
    int m_close_log;    //关闭日志

};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif