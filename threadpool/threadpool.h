#ifndef THREADPOLL_H
#define THREADPOLL_H

#include <list>
#include <cstdio>
#include <pthread.h>
#include <exception>
#include "../thread_sync/locker.h"
#include "../database/sql_connection_pool.h"

/************ 线程池类 ************/
// 定义成类模板是为了代码复用
// 模板参数T是任务类型
template<typename T>
class threadpool{
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T *request);

private:
    // 工作线程上运行的处理函数，它不断从工作队列中取出任务并执行
    // C++中该函数必须为静态
    static void *worker(void *arg);

    // 线程运行函数
    void run();

private:
    int m_thread_number;        // 线程的数量
    pthread_t* m_threads;       // 线程池数组，大小为m_thread_number
    int m_max_requests;         // 请求队列中，最多运行等待处理的请求数量
    std::list<T*> m_workqueue;  // 请求队列
    locker m_queuelocker;       // 保护请求队列的互斥量
    sem m_queuestat;            // 信号量用来判断是否有任务需要处理
    bool m_stop;                // 是否结束线程
    int m_actor_model;          // 模型切换
    connection_pool *m_connPool;// 数据库
};

/************ 构造函数：初始化 ************/
template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : 
    m_actor_model(actor_model), m_connPool(connPool),
    m_thread_number(thread_number), m_max_requests(max_requests), 
    m_stop(false), m_threads(NULL) 
{
    // 初始化的线程数小于1或最大请求数小于1，则抛出错误
    if(thread_number <= 0 || max_requests <= 0){
        throw std::exception();
    }

    // 初始化线程池数组，析构时记得删除
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads){
        throw std::exception();        
    }

    // 创建thread_number个线程，并设置为线程分离（detach）
    // 当detach的线程终止后，会自动释放资源
    for(int i = 0; i < thread_number; i++){
        // 创建线程
        if(pthread_create(&m_threads[i], NULL, worker, this) != 0){
            // 创建线程失败
            delete[] m_threads;     // 删除线程池数组
            throw std::exception(); // 抛出异常
        }

        // 分离线程
        if (pthread_detach(m_threads[i]) != 0)
        {
            // 分离失败：不是一个joinable线程或找不到线程ID
            delete[] m_threads;
            throw std::exception();
        }
    }
}

/************ 析构函数 ************/
template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

/************ 向请求队列中添加请求（任务） ************/
template<typename T>
bool threadpool<T>::append(T* request, int state){
    m_queuelocker.lock();

    // 如果任务队列中的任务数量大于等于最大请求数，
    // 代表无法再添加请求，此时释放锁，并返回false
    if(m_workqueue.size() >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }

    request->m_state = state;

    // 如果没有达到最大请求数，则正常插入任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();    // 信号量加一，代表需要处理的任务加一
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/************ 工作线程所用函数 ************/
template <typename T>
void* threadpool<T>::worker(void *arg)
{
    // 由于静态成员函数无法访问类内非静态的成员函数与成员变量
    // 故通过参数arg从外部传入对象，以访问类内成员函数与变量
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    // 如果未停止，则线程一直循环运行
    while (!m_stop)
    {
        // 信号量减一，表示取出任务并将执行
        // 否则将阻塞在此
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())        // 如果请求队列为空，则释放锁，并直接进行下一次循环
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front(); // 从队头取出任务
        m_workqueue.pop_front();          // 取出后要删除队列中的任务
        m_queuelocker.unlock();           // 取出任务后释放锁，以便其他线程从队列中取任务
        if (!request){                    // 如果没有获取到任务，直接进行下一次循环
            continue;
        }
        if (1 == m_actor_model)
        {
            // 读
            if (0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                // 无数据可读
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            // 写
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                // 发送失败或缓冲区已无需要发送的数据
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}


#endif