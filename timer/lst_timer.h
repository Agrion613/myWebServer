#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

// 前向申明
// 在使用时，只能定义该类的指针
// 见38行代码
class util_timer;

// 客户端的连接数据
// 包括：客户端地址、与该客户端对应的文件描述符以及定时器
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

// 定时器类，这里由双向链表连接所有定时器
// 所以，该类需要有的成员有：
// 1.下一个节点与前一个节点
// 2.该定时器的超时时间
// 3.该定时器所需处理的定时事件（回调函数）
//   具体的，从内核事件表删除事件，关闭文件描述符，释放连接资源
// 4.与该定时器相关的客户端数据（每创建一个连接就对应一个定时器）
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    // 对应2.
    time_t expire;
    // 对应3.
    void (* cb_func)(client_data *);
    // 对应4.
    client_data *user_data;
    
    // 对应1.
    util_timer *prev;
    util_timer *next;
};

// 定时器容器，将所有定时器组合起来统一管理
// 涉及的成员函数应该有：
// 1.添加定时器（这个好理解）（使用双向链表，按超时时间升序排列）
// 2.调整定时器（比如在定时等待期间，有收发数据的行为，
//   那么对应文件描述符的定时器应该被调整至双向链表中的其他合适位置）
// 3.删除定时器
// 4.定时器任务处理函数 tick()
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;
    util_timer *tail;
};

// 定时方法与信号通知流程：
// 使用SIGALRM信号定时
// 信号通知流程：
//   信号处理函数仅仅发送信号通知程序主循环，
//   将信号对应的处理逻辑放在程序主循环中，由主循环执行信号对应的逻辑代码

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif
