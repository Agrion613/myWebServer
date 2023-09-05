#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H


#include <iostream>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include "../thread_sync/locker.h"

// 数组中数据的类型可能未定，使用类模板
template<typename T>
class block_queue{
public:
    block_queue(int max_size = 10000){ // 默认最大长度为10000
        if(max_size <= 0) {
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    // 析构函数涉及删除数组（公共资源），所以需要加锁
    ~block_queue(){
        m_mutex.lock();
        if(m_array != NULL){
            delete[] m_array;
        }
        m_mutex.unlock();
    }

    void clear(){
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    // 判断是否满了，由于在判断的时候，别的线程可能会对队列进行删除或增加，
    // 所以需要加锁
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断是否为空，由于在判断的时候，别的线程可能会对队列进行删除或增加，
    // 所以需要加锁   
    bool empty(){
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;        
    }

    // 返回队首元素
    // 传入参数：value，传出参数：value
    bool front(T& value){
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 返回队尾元素
    bool back(T& value){
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    // 返回数组当前大长度
    int size(){
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    // 返回数组最大长度
    int max_size(){
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素，需要先将所有使用队列的线程唤醒
    // 当有元素进入队列时，相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量，则唤醒无意义
    bool push(const T& item){
        m_mutex.lock();
        // 如果队列已满，则无法插入新的元素
        // 在唤醒所有使用队列的线程后就释放锁并返回
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        // 实现循环队列
        // 向队尾插入元素
        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;

        m_size++;

        // 广播唤醒所有使用队列的线程，代表此时已经有数据可用
        // 则被唤醒的消费者线程可以将元素从队列中取出
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    // 消费者线程需要从队列中取元素时，若当前队列无元素
    // 则会阻塞等待条件变量
    bool pop(T& item){
        m_mutex.lock();
        // 无元素则一直等待
        while(m_size <= 0){
            // 阻塞等待条件变量，若被唤醒后发现依旧没有可使用的条件变量
            // 则释放锁（因为该函数被唤醒后会自动获得锁）然后返回
            if(!m_cond.wait(m_mutex.get())){ 
                m_mutex.unlock();
                return false;
            }
        }

        // 实现循环队列，从队首取出元素
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    // 增加消费者线程的超时处理，若超时ms_timeout(毫秒)则不继续等待
    // 所以此时使用if判断，而不是未做超时处理时的while循环
    bool pop(T& item, int ms_timeout){
        struct timespec t = {0, 0};  // 第一个是秒，第二个是纳秒
        struct timeval now = {0, 0}; // 第一个是秒，第二个是微秒
        gettimeofday(&now, NULL);    // 获取当前时间

        m_mutex.lock();

        if(m_size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout / 1000; // 使用现在的时间加上超时等待时间，可得到不再继续等待的时间（单位：s）
            t.tv_nsec = (ms_timeout % 1000) * 1000;    // 处理小于1000ms的零头
            if(!m_cond.timedwait(m_mutex.get(), t)){
                // 如果超时了，则释放锁然后返回
                m_mutex.unlock();
                return false;
            }
        }
        
        // 如果被唤醒后发现可取元素还是为0
        // 则直接释放锁然后返回
        if(m_size <= 0){
            m_mutex.unlock();
            return false;
        }

        // 实现循环队列，从队首取出元素
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_cond;

    T* m_array;
    int m_size;     // 阻塞队列中已有数据的长度
    int m_max_size; // 阻塞队列的最大长度
    int m_front;    // 队首元素的前一个索引
    int m_back;     // 队尾元素的索引
};

#endif