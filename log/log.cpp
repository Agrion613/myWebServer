#include "./log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if(m_fp != NULL){
        fclose(m_fp);
    }
}
// 异步写入需要设置阻塞队列长度，同步不需要
bool Log::init(const char *file_name, int close_log, int log_buf_size, int spilt_lines, int max_queue_size){
    // 如果设置了max_queue_size不为0，则代表使用异步
    // 设置是否同步标志，以及创建队列，最后创建异步写线程
    if(max_queue_size >= 1){
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        // flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];    // 设置日志缓冲区
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = spilt_lines;

    // 获取时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 查找file_name中最后一次出现字符'/'的位置（从左至右查找）并返回其地址
    // 其实是为了获取日志文件的名字，因为最后一个'/'后面就是日志名字
    // 如果返回NULL则代表没有找到，也表示输入的file_name只是一个名字，并不是路径，不能从file_name中获取日志名字
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // file_name为空
    if(p == NULL){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }else{
        strcpy(log_name, p + 1); // 将获取到的日志名字拷贝进log_name中
        strncpy(dir_name, file_name, p - file_name + 1); // 将日志名字前面的路径拷贝进dir_name中
                                                         // p - file_name + 1表示p所指的位置在file_name中的索引（从1开始）
                                                         // 也就是日志名字前的路径长度
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    // 打开文件，若不存在则创建
    m_fp = fopen(log_full_name, "a");
    if(m_fp == NULL){
        return false;
    }
    return true;
}

// 根据日志等级写入日志
void Log::write_log(int level, const char *format, ...){
    // 获取时间，以下三句等于
    // time_t t = time(NULL);
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;

    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    m_mutex.lock();
    m_count++; // 日志行数加一

    // 已经到了第二天或者日志中的行数已经大于设置的最大行数
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        
        char new_log[256] = {0};
        // 刷新缓冲区：将缓冲区里数据强制写入m_fp中
        fflush(m_fp);
        // 关闭前一天的日志文件
        fclose(m_fp);
        // 新日志的名字
        char tail[16] = {0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // 情况一：已经到了第二天
        if (m_today != my_tm.tm_mday)
        {
            // 写入新的日志文件名
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            // 更新m_today
            m_today = my_tm.tm_mday;
            // 新日志文件中的初始日志行数为0
            m_count = 0;
        }
        // 情况二：日志中的行数已经大于设置的最大行数
        else
        {
            // 新日志的名字为：路径名+日志文件格式化名（年月日）+日志名.日志编号
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }

        // 创建这个新日志文件
        m_fp = fopen(new_log, "a");
    }
    // 创建完成后需要释放锁
    m_mutex.unlock();

    // 创建可变参数列表
    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式至缓冲区m_buf
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 从m_buf + n这个地址开始写入格式化信息，长度为m_log_buf_size - 1，
    // 留一个长度是因为函数会自动在最后填充'\0'
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    // 日志写完后换行
    m_buf[n + m] = '\n';
    // 换行后以'\0'结束
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    // 如果是异步写入且日志队列没有满，则将日志插入队列中
    // 写日志线程会不断将日志队列中的日志写入日志文件
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }

    // 其他情况：直接写入日志文件
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);

}

void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}