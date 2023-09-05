#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
/************ 互斥锁类 ************/
class locker{
public:
    locker()
    {
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            // 如果初始化失败，抛出异常
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    // 获取锁
    bool lock(){
        return pthread_mutex_lock(&m_mutex);
    }
    // 释放锁
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex);
    }
    pthread_mutex_t* get(){
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

/************ 条件变量类 ************/
// 条件变量可以引起阻塞但不是锁
// 要和互斥锁配合使用
class cond{
public:
    cond(){
        if (pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    // 条件变量阻塞等待
    bool wait(pthread_mutex_t* mutex){  // 一旦进入wait状态就会自动释放mutex
                                        // 并阻塞当前线程，直到别的线程使用pthread_cond_signal(&cond);唤醒
                                        // 唤醒后，又会自动获得mutex
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }
    // 超时等待
    bool timedwait(pthread_mutex_t* mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }
    // 唤醒至少一个阻塞在条件变量上的线程
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    // 唤醒所有阻塞在条件变量上的线程
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    pthread_cond_t m_cond;
};

/************ 信号量类 ************/
class sem{
public:
    // 初始化信号量的值为0
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }
    // 初始化信号量的值为num
    sem(int num){
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    // 等待信号量（减少信号量）
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }
    // 增加信号量
    bool post(){
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

#endif