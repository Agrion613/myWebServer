#include "lst_timer.h"
#include "../http/http_conn.h"

/******************************定时器容器类*************************************/
// 定时器容器类的构造函数，即，将头尾指针置空
sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL;
}

// 定时器容器类的析构函数，删除链表上的所有数据，即删除所有的定时器
sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 向双向链表中添加定时器
void sort_timer_lst::add_timer(util_timer *timer){
    if(timer == NULL) return;

    // 如果链表中头节点为空
    if(head == NULL){
        head = timer;
        tail = timer;
        return;
    }

    // 如果链表不为空，且插入的定时器超时时间小于链表头节点
    // 则直接插入头部
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    
    // 其他情况需要插入在链表之中，
    // 调用私有成员函数（同名的重载函数）
    add_timer(timer, head);

}

// 调整一个定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer){
    if(timer == NULL) return;
    util_timer *tmp = timer->next;

    // 当这个定时器在链表末尾时，不需要调整，
    // 因为它本来就在最后，就代表它在需要调整之前就是超时时间最长的定时器，
    // 当它需要被调整就代表超时时间重新计算，因此还是在最末尾处
    if(tmp == NULL || (timer->expire < tmp->expire)){
        return;
    }

    // 如果在头部，则直接先断开该头节点，再插入，
    // 相当于重新插入
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    // 如果不在头部，与上同理，也是先取出这个定时器
    // 再重新插入
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

// 删除定时器h
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (timer == NULL)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 定时器任务处理函数：
// 判断链表中超时的定时器，
// 若超时则调用回调函数对客户端资源进行释放
// 然后删除链表中的该定时器
void sort_timer_lst::tick(){
    if(head == NULL) return;

    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp){
        if(cur < tmp->expire){
            // 如果第一个定时器都没有超时
            // 那么其他的定时器也不可能超时，直接结束循环
            break;
        }

        // 调用回调函数释放资源
        tmp->cb_func(tmp->user_data);

        head = head->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

// 私有成员函数，用于找到链表中的正确位置插入定时器
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp){
        // 找到了该插入的位置
        if(timer->expire < tmp->expire){ 
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}


/******************************定时方法与信号通知流程相关的函数*************************************/
// 初始化，传入超时时间
void Utils::init(int timeslot){
    m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
// 返回旧的文件描述符（保护现场）
int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, new_option);
    return old_option;
}

// 将要检测的文件描述符加入epoll，注册读事件，ET模式，并开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    // 1 代表ET模式
    if(1 == TRIGMode){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 捕捉到信号时触发的信号处理函数
// 实际上并不进行处理，而是将捕捉到信号通知给主循环
// 真正的信号处理函数在主循环中
// 这是为了避免信号被屏蔽太久
void Utils::sig_handler(int sig){
    // 为了保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;

    // 向主循环通知
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;

    // SA_RESTART标志位：
    // 如果信号中断了进程的某个系统调用，则系统自动启动该系统调用
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }

    // 初始化信号集，把所有的信号加入到此信号集里，即将所有的信号标志位置为1
    // 所以这里为屏蔽所有信号
    sigfillset(&sa.sa_mask);

    // 设置信号捕捉函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

// 静态成员变量在类外初始化
int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

/******************************定时器容器类中调用的回调函数*************************************/
// 作用：释放客户端连接资源
class Utils;
void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}