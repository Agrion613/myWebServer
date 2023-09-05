# 1. 五种I/O模型

- **阻塞I/O**：当系统调用时，可能因为无法立即完成而被操作系统挂起，直到等待的事件发生为止

  ![image-20220618164532931](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164532931.png)

- **非阻塞I/O**：执行的系统调用总是立即返回，不管事件是否已经发生
  - 只有在事件已经发生的情况下操作非阻塞I/O(读写等)，才能提高程序的效率。因此非阻塞I/O通常要和其他I/O通知机制一起使用，比如I/O复用和SIGIO信号

  ![image-20220618164556964](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164556964.png)

- **I/O复用**：应用程序通过I/O复用函数向内核注册一组事件，内核通过I/O复用函数把其中**就绪的**事件通知给应用程序
  - **I/O多路复用使得程序能同时监听多个文件描述符，能提高程序性能**
  - 常用的I/O复用函数：`select`、`poll`和`epoll_wait`
  - I/O复用函数本身是阻塞的，它们提高程序效率的原因在于它们具有同时监听多个I/O事件的能力
  - 注意它只是为了能同时监听多个客户端的数据，并不代表能承受高并发
  - 高并发依旧要使用多进程/多线程
  
  ![image-20220618164625493](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164625493.png)
  
- **SIGIO信号**：SIGIO信号也可以用来报告IO事件。可以为一个目标文件描述符指定宿主进程，那么被指定的宿主进程将捕获到SIGIO信号。这样，当目标文件描述符上有事件发生时，SIGIO信号的信号处理函数将被触发，从而可以在该信号处理函数中对目标文件描述符执行非阻塞I/O操作

  ![image-20220618164639621](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164639621.png)

- **异步I/O**

  - 不同于同步模型，异步模型的拷贝行为由内核完成

  ![image-20220618164659108](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164659108.png)

**总结：**

- 同步和异步的区别可以这样理解：同步IO向应用程序通知的是IO就绪事件，而异步IO向应用程序通知的是IO完成事件

|  I/O模型  |                      读写操作和阻塞阶段                      |
| :-------: | :----------------------------------------------------------: |
|  阻塞I/O  |                      程序阻塞于读写函数                      |
|  I/O复用  | 程序阻塞于I/O复用系统调用，但可同时监听多个IO事件。对I/O本身的读写操作是非阻塞的 |
| SIGIO信号 | 信号触发读写就绪事件，用户程序执行读写操作。程序没有阻塞阶段 |
|  异步I/O  |     内核执行读写操作并触发读写完成事件。程序没有阻塞阶段     |



# 2. HTTP协议

- 应用层协议

## 2.1 HTTP请求/响应

![image-20220618164408985](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164408985.png)

- HTTP协议是基于TCP/IP协议之上的应用层协议，基于**请求-响应**模式。HTTP请求都是从客户端发起，最后由服务器端响应请求并返回，也就是说服务器在接收到请求之前不会发送响应

### 2.1.1 HTTP协议请求报文格式

![image-20220618164425990](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164425990.png)

- 请求行
- 请求头部
- 请求空行
- 请求数据

### 2.1.2 HTTP协议请求报文格式

![image-20220618164440023](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618164440023.png)

- 状态行
- 响应头部
- 响应空行
- 响应正文

## 2.2 HTTP协议请求方法

![image-20220618165454554](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618165454554.png)

# 3. 服务器编程基本框架

- 所有服务器的基本框架都一样，不同之处在于逻辑处理

![image-20220618170052116](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618170052116.png)

| 模块         | 功能                       |
| ------------ | -------------------------- |
| I/O处理单元  | 处理客户连接，读写网络数据 |
| 逻辑单元     | 业务进程或线程             |
| 网络存储单元 | 数据库、文件或缓存         |
| 请求队列     | 各单元间的通信方式         |

- I/O处理单元是服务器管理客户连接的模块，它通常完成以下工作：等待并接受新的客户连接，接收客户数据，将服务器响应数据返回给客户端。**但是数据收发不一定在这个单元中执行，具体要看使用了哪种事件处理模式**
- 逻辑单元通常是线程或进程，它分析并处理客户端数据，然后将结果传递给I/O处理单元或直接发送给客户端，**具体还是要看使用了哪种事件处理模式**。服务器通过拥有多个逻辑单元，以实现对多个客户端任务的并发处理。
- 网络存储单元是数据库、缓存、文件等，但不是必须的
- 请求队列是各单元之间通信方式的抽象：I/O处理单元接收到客户端请求时，需要以某种方式通知一个逻辑单元来处理该请求。同样，多个逻辑单元同时访问一个存储单元时，也需要采用某种机制来协调处理竞态条件。**请求队列通常被实现为<font color=red>池</font>的一部分**



## 3.1 两种事件处理模式

- 服务器程序通常需要处理三类事件：**I/O事件**、**信号**以及**定时事件**
- Reactor模式使用同步I/O实现
- Proactor模式使用异步I/O实现

### 3.1.1 Reactor模式

![image-20220617163721778](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220617163721778.png)

![image-20220617163954297](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220617163954297.png)





### 3.1.2 Proactor模式

- 与Reactor模式不同，Proactor模式将所有I/O操作都交给主线程和内核来处理，工作线程仅仅负责业务逻辑。

![image-20220617170504398](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220617170504398.png)

![image-20220617170521948](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220617170521948.png)

![image-20220617170625962](C:\Users\stan\AppData\Roaming\Typora\typora-user-images\image-20220617170625962.png)

### 3.1.3 使用同步I/O模式Proactor模式

- 基本原理：主线程执行数据读写操作，完成读写后，**主线程向工作线程通知读写完成事件**。那么从工作线程的角度来看，它们就直接获得了数据读写的结果，接下来只需要对读写数据的结果进行逻辑处理即可，这符合3.1.2节中的Proactor模式的处理流程
- 模拟流程如下：
  1. 主线程往`epoll`内核事件表中注册socket上的读就绪事件；
  2. 主线程调用`epoll_wait`等待socket上的读就绪事件；
  3. 当socket上有数据可读时，`epoll_wait`通知主线程。主线程从socket循环读取数据，直到没有数据可读，**然后将读取到的数据封装成一个请求对象并插入请求队列**；
  4. 睡眠在请求队列上的某个工作线程被唤醒，它获取请求对象并处理客户端请求，然后往`epoll`内核事件表中注册socket上的写就绪事件；
  5. 主线程调用`epoll_wait`等待socket可写；
  6. 当socket可写时，`epoll_wait`通知主线程。主线程往socket上写入服务器处理客户请求的结果。

![image-20220618180848443](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618180848443.png)

# 4. 日志系统

- 概述：使用单例模式创建日志系统，对服务器运行状态、错误信息和访问数据进行记录；可分为同步和异步两种写入方式
- 同步日志：写入函数与工作线程串行执行，由于涉及I/O操作，当单条日志量较大时，同步模式会阻塞整个处理流程，导致服务器所能处理的并发能力下降
- 异步日志：将所写的日志先存入阻塞队列中，写线程从阻塞队列中取出日志
- **所以，日志系统包括两部分**：
  - 阻塞队列的编程
    - 将生产者-消费者模组进行封装，**使用循环数组实现队列**，作为消费者和生产者的缓冲区
    - <font color=red>**为什么要使用循环队列？**</font>
      - 因为插入元素都是往队尾插入，所以当队尾已经到头，但是队首方向还有空间的时候，使用循环队列可以在队首方向插入新数据，这样就模拟了动态的队首和队尾，从而实现了循环队列
      - 如果不使用循环队列，可能会导致插入的时候超出数组索引从而无法插入（但实际上数组还有空间）
  - 日志的写入编程
    - 使用单例模式创建日志

## 4.1 单例模式

- 保证一个类只创建一个实例，同时提供全局访问的方法
- 实现思路：
  - **私有化其构造函数**，以防外界创建单例类的对象；使用类的私有静态指针变量指向类的唯一实例，**并用一个公有的静态方法以获取该实例**
- 实现方式：
  - 懒汉模式：在不使用的时候不去初始化，只有在第一次使用的时候才进行初始化
  - 饿汉模式：在程序运行时立即初始化

### 4.1.1 经典的线程安全懒汉模式

- 使用双检测锁模式保证懒汉模式的线程安全

```c++
class single{
private:
    //私有静态指针变量指向唯一实例
    static single *p;

    //静态锁，是由于静态函数只能访问静态成员
    static pthread_mutex_t lock;

    //私有化构造函数
    single(){
        pthread_mutex_init(&lock, NULL);
    }
    ~single(){}

public:
    //公有静态方法获取实例
    static single* getinstance();

};

pthread_mutex_t single::lock;

single* single::p = NULL;
single* single::getinstance(){
    if (NULL == p){
        pthread_mutex_lock(&lock);
        if (NULL == p){
            p = new single;
        }
        pthread_mutex_unlock(&lock);
    }
    return p;
}
```

- 为什么要用双检测？
  - 如果只检测一次，在每次调用获取实例的方法时，都需要加锁，这将严重影响程序性能。双层检测可以有效避免这种情况，仅在第一次创建单例的时候加锁，其他时候都不再符合NULL == p的情况，直接返回已创建好的实例。

### 4.1.2 局部静态变量之线程安全懒汉模式

- 《Effective C++》（Item 04）中的提出另一种更优雅的单例模式实现，使用函数内的局部静态对象，这种方法不用加锁和解锁操作

```C++
class single{
private:
    single(){}
    ~single(){}

public:
    static single* getinstance();
};

single* single::getinstance(){
    static single obj;
    return &obj;
}
```

> **<font color=red>其实，C++0X以后，要求编译器保证内部静态变量的线程安全性，故C++0x之后该实现是线程安全的，C++0x之前仍需加锁，其中C++0x是C++11标准成为正式标准之前的草案临时名字。所以，如果使用C++11之前的标准，还是需要加锁，以下为加锁的版本</font>**

```C++
class single{
private:
    static pthread_mutex_t lock;
    single(){
        pthread_mutex_init(&lock, NULL);
    }
    ~single(){}

public:
    static single* getinstance();

};
pthread_mutex_t single::lock;
single* single::getinstance(){
    pthread_mutex_lock(&lock);
    static single obj;
    pthread_mutex_unlock(&lock);
    return &obj;
}
```

### 4.1.3 饿汉模式

- 饿汉模式不需要用锁，就可以实现线程安全。原因在于，在程序运行时就定义了对象，并对其初始化。之后，不管哪个线程调用成员函数`getinstance()`，都只不过是返回一个对象的指针而已。所以是线程安全的，不需要在获取实例的成员函数中加锁

```c++
class single{
private:
    static single* p;
    single(){}
    ~single(){}

public:
    static single* getinstance();

};
single* single::p = new single();
single* single::getinstance(){
    return p;
}

//测试方法
int main(){

    single *p1 = single::getinstance();
    single *p2 = single::getinstance();

    if (p1 == p2)
        cout << "same" << endl;

    system("pause");
    return 0;
}
```

> <font color=red>**饿汉模式虽好，但其存在隐藏的问题，在于非静态对象（函数外的static对象）在不同编译单元中的初始化顺序是未定义的。如果在初始化完成之前调用 `getInstance()`方法会返回一个未定义的实例**</font>

## 4.2 阻塞队列：生产者-消费者模式

- 该模式使用条件变量(`cond`)实现，注意条件变量并不是锁，所以需要搭配互斥锁使用

### 4.2.1 条件变量

- 基础API

![image-20220621192024911](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220621192024911.png)

### 4.2.2 条件变量注意事项

- 使用`pthread_cond_wait()`前需要加互斥锁，这是为了在多线程访问中避免资源竞争
- 使用`pthread_cond_wait()`后，线程会阻塞，但此时线程依旧持有锁，如果不释放锁，会使得其他线程无法访问公共资源，所以`pthread_cond_wait()`内部实现中，一旦该函数被调用，会自动释放持有的锁。因此回到上一点，在调用`pthread_cond_wait()`之前，必须持有锁
- 当调用`pthread_cond_wait()`的线程被`pthread_cond_signal()`或`pthread_cond_broadcast()`唤醒后，又会自动持有锁

## 4.3 日志类的定义与使用

### 4.3.1 基础API

- `fputs`

  ```C++
  #include <stdio.h>
  int fputs(const char *str, FILE *stream);
  ```

  - `str`：数组，包含了要写入的以空字符终止的字符序列
  - `stream`：指向`FILE`对象的指针，该对象标识了要被写入字符串的流

- 可变参数宏`__VA_ARGS__`

  - 是一个可变参数的宏，定义时宏定义中参数列表的最后一个参数为省略号，**在实际使用时会发现有时会加##，有时又不加**。

  ```c++
  #include <stdlib.h>
  //最简单的定义
  #define my_print1(...)  printf(__VA_ARGS__)
  
  //搭配va_list的format使用
  #define my_print2(format, ...) printf(format, __VA_ARGS__)  
  #define my_print3(format, ...) printf(format, ##__VA_ARGS__)
  ```

  - 宏前面加上`##`的作用在于，当可变参数的个数为0时，这里`printf`参数列表中的`##`会把前面多余的","去掉，否则会编译出错，<font color=red>**建议使用后面这种，使得程序更加健壮**</font>。

- `fflush`

  ```c++
  #include <stdio.h>
  int fflush(FILE *stream);
  ```

  - `fflush()`会强迫将缓冲区内的数据写回参数`stream`指定的文件中，如果参数`stream` 为`NULL`，`fflush()`会将所有打开的文件数据更新
  - 在使用多个输出函数连续进行多次输出到控制台时，有可能下一个数据在上一个数据还没输出完毕（还在输出缓冲区中）时，下一个`printf`就把另一个数据加入输出缓冲区，结果冲掉了原来的数据，出现输出错误
  - 在`prinf()`后加上`fflush(stdout);` **强制马上输出到控制台，可以避免出现上述错误**

### 4.3.2 流程

- 如何获取实例？
  - 单例模式，使用局部变量的懒汉模式获取实例
- 日志文件的写入方式分类？
  - 同步：
    - 判断是否需要分文件（新文件）
    - 直接格式化输出内容，将信息写入日志
  - 异步：
    - 判断是否需要分文件
    - 格式化输出内容，将内容写入阻塞队列，**创建一个写线程**，从阻塞队列中取出内容写入日志文件

### 4.3.3 日志类所需函数定义

- 包括但不限于以下方法：
  - 公有的实例获取方法
  - 初始化日志文件方法（包括但不限于：日志文件、日志缓冲区大小、最大行数、最长日志条队列）
  - 异步日志写入方法
  - 内容格式化方法
  - 缓冲区刷新
  - .....

### 4.3.4 一些notes

- 因为`sprintf`可能导致**缓冲区溢出问题**而不被推荐使用，所以在项目中优先选择使用`snprintf`函数，虽然会稍微麻烦那么一点点。这里就是`sprintf`和`snprintf`最主要的区别：`snprintf`通过提供缓冲区的可用大小传入参数来保证缓冲区的不溢出，如果超出缓冲区大小则进行截断。但是对于`snprintf`函数，还有一些细微的差别需要注意：

  ![image-20220623205856773](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220623205856773.png)

  **<font color=red>其他细节：</font>**https://blog.csdn.net/Alex123980/article/details/51813893

# 5. 数据库连接系统

- 主要内容：
  - 单例模式创建
  - **`RAII`机制**（资源获取即初始化）释放数据库连接

## 5.1 数据库连接池：初始化

- 需要注意，使用RAII是为了保证释放资源时安全，所以销毁连接池时没有直接被外部调用，而是通过RAII机制完成自动释放
- 使用信号量实现多线程争夺连接的同步机制，将信号量初始化为数据库的连接总数

## 5.2 数据库连接池：获取、释放连接

- 当线程数量大于数据库连接数量时，使用信号量进行同步，每次取出连接，信号量原子减1，释放连接原子加1，若连接池内没有连接了，则阻塞等待。另外，由于多线程操作连接池，会造成竞争，使用互斥锁完成同步

## 5.3 数据库连接池：销毁连接池

- 通过迭代器遍历连接池链表，关闭对应数据库连接，清空链表并重置空闲连接和现有连接数量

## 5.4 RAII机制

- 不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行自动获取和释放。

# 6. 定时器：用以处理非活动连接

## 6.1 基础知识

- 非活跃：是指客户端与服务器建立连接后，**长时间不交换数据，一直占用服务器端的文件描述符**，导致资源浪费
- 定时事件：是指固定一段时间后触发某段代码，该段代码会处理一个事件，如：从内核中删除事件，并关闭文件描述符，释放连接
- 定时器：是指将多种定时事件封装起来，在该项目中，具体只涉及一种定时事件：即定期检测非活跃连接。**这里将该定时事件与连接资源封装为一个结构体定时器**
- 定时器容器：是指使用某种容器类数据结构，将上述多个定时器组合起来，便于对定时事件统一管理，具体的，该项目中使用**升序链表**将所有定时器串联组织起来

## 6.2 Linux中的三种定时方法

- `socket`选项`SO_RECVTIMEO`和`SO_SNDTIMEO`

- `SIGALRM`信号

- I/O复用系统调用的超时参数

  

- 该项目中使用`SIGALRM`信号实现定时。具体的，利用`alarm`函数周期性地触发`SIGALRM`信号，信号处理函数利用管道通知主循环，主循环接收到该信号后对升序链表上所有定时器进行处理，若该段时间内没有数据交换，则将关闭连接。释放所占用资源

## 6.3 基础API

### 6.3.1 `sigaction`结构体

```C++
struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
}
```

- `sa_handler`是一个函数指针，指向信号处理函数

- `sa_sigaction`同样是信号处理函数，有三个参数，可以获得关于信号更详细的信息

- `sa_mask`用来指定在信号处理函数执行期间需要被屏蔽的信号

- `sa_flags`用于指定信号处理的行为

- - `SA_RESTART`，使被信号打断的系统调用自动重新发起
  - `SA_NOCLDSTOP`，使父进程在它的子进程暂停或继续运行时不会收到 SIGCHLD 信号
  - `SA_NOCLDWAIT`，使父进程在它的子进程退出时不会收到 SIGCHLD 信号，这时子进程如果退出也不会成为僵尸进程
  - `SA_NODEFER`，使对信号的屏蔽无效，即在信号处理函数执行期间仍能发出这个信号
  - `SA_RESETHAND`，信号处理之后重新设置为默认的处理方式
  - `SA_SIGINFO`，使用` sa_sigaction `成员而不是` sa_handler `作为信号处理函数

- `sa_restorer`一般不使用

### 6.3.2 `sigaction`函数

```C++
#include <signal.h>

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```

- `signum`表示操作的信号。
- `act`表示对信号设置新的处理方式。
- `oldact`表示信号原来的处理方式。
- 返回值，0 表示成功，-1 表示有错误发生。

### 6.3.3 `sigfillset`函数

- 用来将参数set信号集初始化，然后把所有的信号加入到此信号集里

```C++
#include <signal.h>

int sigfillset(sigset_t *set);
```

### 6.3.4 `SIGALRM`与`SIGTERM`

```C++
#define SIGALRM  14     //由alarm系统调用产生timer时钟信号
#define SIGTERM  15     //终端发送的终止信号
```

### 6.3.5 `alarm`函数

- 设置信号传送闹钟，即用来设置信号`SIGALRM`在经过参数`seconds`秒数后发送给目前的进程。**如果未设置信号`SIGALRM`的处理函数，那么`alarm()`默认处理终止进程**

```C++
#include <unistd.h>;

unsigned int alarm(unsigned int seconds);
```

### 6.3.6 `socketpair`函数

- 在Linux下，使用`socketpair`函数能够**创建一对套接字进行通信**，项目中使用管道通信。

```C++
#include <sys/types.h>
#include <sys/socket.h>

int socketpair(int domain, int type, int protocol, int sv[2]);
```

- `domain`表示协议族，PF_UNIX或者AF_UNIX
- `type`表示协议，可以是`SOCK_STREAM`或者`SOCK_DGRAM`，**`SOCK_STREAM`基于TCP，`SOCK_DGRAM`基于UDP**
- `protocol`表示类型，只能为0
- `sv[2]`表示套节字柄对，**<font color=red>该两个句柄作用相同，均能进行读写双向操作</font>**
- 返回结果， 0为创建成功，-1为创建失败

### 6.3.7 `send`函数

```C++
#include <sys/types.h>
#include <sys/socket.h>

ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

- 当套接字发送缓冲区变满时，`send`通常会阻塞，除非套接字设置为非阻塞模式，当缓冲区变满时，返回`EAGAIN`或者`EWOULDBLOCK`错误，此时可以调用`select`函数来监视何时可以发送数据。

## 6.4 信号通知流程

- Linux下的信号采用的**异步处理机制**，信号处理函数和当前进程是两条不同的执行路线。具体的，当进程收到信号时，**操作系统会中断进程当前的正常流程，转而进入信号处理函数执行操作，完成后再返回中断的地方继续执行**。

- **为避免信号竞态现象发生，<font color=red>信号处理期间系统不会再次触发它</font>**。所以，为确保该信号不被屏蔽太久，信号处理函数需要**尽可能快地执行完毕**。

- 一般的信号处理函数需要处理该信号对应的逻辑，当该逻辑比较复杂时，信号处理函数执行时间过长，会导致信号屏蔽太久，这里的解决方案是：
  - 信号处理函数仅仅发送信号通知程序主循环，将信号对应的处理逻辑放在程序主循环中，由主循环执行信号对应的逻辑代码。

### 6.4.1 信号处理流程

- 信号的接收

- - 接收信号的任务是**由内核代理**的，当内核接收到信号后，会将其放到对应进程的信号队列中，同时向进程发送一个中断，使其陷入内核态。**注意，此时信号还只是在队列中，对进程来说暂时是不知道有信号到来的**。

- 信号的检测

- - 进程陷入内核态后，有两种场景会对信号进行检测：
    - 进程从内核态返回到用户态前进行信号检测
    - 进程在内核态中，从睡眠状态被唤醒的时候进行信号检测

- - 当发现有新信号时，便会进入下一步，信号的处理。

- 信号的处理

- - ( **内核** )信号处理函数是运行在用户态的，调用处理函数前，内核会将当前内核栈的内容备份拷贝到用户栈上，**并且修改指令寄存器（`eip`）将其指向信号处理函数**。
  - ( **用户** )接下来进程返回到用户态中，执行相应的信号处理函数。
  - ( **内核** )信号处理函数执行完成后，还需要返回内核态，检查是否还有其它信号未处理。
  - ( **用户** )如果所有信号都处理完成，就会将内核栈恢复（从用户栈的备份拷贝回来），同时恢复指令寄存器（`eip`）将其指向中断前的运行位置，最后回到用户态继续执行进程。

# 7. http连接

## 7.1 基础接口知识

### 7.1.1 epoll基础函数

- **epoll_create函数**

  ```C++
  #include <sys/epoll.h>
  int epoll_create(int size)
  ```

  创建一个指示epoll内核事件表的文件描述符，该描述符将用作其他epoll系统调用的第一个参数，**size不起作用**。

- **epoll_ctl函数**

  ```C++
  #include <sys/epoll.h>
  int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
  ```

  该函数用于操作内核事件表监控的文件描述符上的事件：**注册、修改、删除**

  - epfd：为epoll_creat的句柄

  - op：表示动作，用3个宏来表示：

  - - EPOLL_CTL_ADD (注册新的fd到epfd)，
    - EPOLL_CTL_MOD (修改已经注册的fd的监听事件)，
    - EPOLL_CTL_DEL (从epfd删除一个fd)；

  - event：告诉内核需要监听的事件

  **上述event是epoll_event结构体指针类型，表示内核所监听的事件，具体定义如下：**

  ```C++
  struct epoll_event {
  	__uint32_t events; /* Epoll events */
  	epoll_data_t data; /* User data variable */
  };
  ```

  - events描述事件类型，其中epoll事件类型有以下几种
    - EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
    - EPOLLOUT：表示对应的文件描述符可以写
    - EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）
    - EPOLLERR：表示对应的文件描述符发生错误
    - EPOLLHUP：表示对应的文件描述符被挂断；
    - EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
    - EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里

- 

- **epoll_wait函数**

  ```C++
  #include <sys/epoll.h>
  int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
  ```

  该函数用于等待所监控文件描述符上有事件的产生，返回就绪的文件描述符个数

  - events：用来存内核得到事件的集合，

  - maxevents：告之内核这个events有多大，这个maxevents的值不能大于创建epoll_create()时的size，

  - timeout：是超时时间

  - - -1：阻塞
    - 0：立即返回，非阻塞
    - \>0：指定毫秒

  - 返回值：成功返回有多少文件描述符就绪，时间到时返回0，出错返回-1



### 7.1.2 select/poll/epoll的区别

- 调用函数：
  - select和poll都是**一个函数**，epoll是**一组函数**
- 文件描述符数量：
  - select通过**线性表**描述文件描述符集合，文件描述符有上限，一般是1024，**但可以修改源码，重新编译内核，不推荐**
  - poll是**链表**描述，突破了文件描述符上限，最大可以打开文件的数目
  - epoll通过**红黑树**描述，最大可以打开文件的数目，可以通过命令`ulimit -n number`修改，**仅对当前终端有效**
- 将文件描述符从用户传给内核：
  - select和poll通过将所有文件描述符拷贝到内核态，**每次调用都需要拷贝**
  - epoll通过epoll_create建立一棵红黑树，通过epoll_ctl将要监听的文件描述符**注册到红黑树上**
- 内核判断就绪的文件描述符：
  - select和poll通过**遍历文件描述符集合**，判断哪个文件描述符上有事件发生
  - epoll_create时，内核除了帮我们在epoll文件系统里建了个红黑树用于存储以后epoll_ctl传来的fd外，还会再建立一个list链表，**用于存储准备就绪的事件**，当epoll_wait调用时，**仅仅观察这个list链表里有没有数据即可**。
  - epoll是根据每个fd上面的回调函数(中断函数)判断，只有发生了事件的socket才会主动的去调用 callback函数，其他空闲状态socket则不会，**若是就绪事件，插入list**
- 应用程序索引就绪文件描述符：
  - **select/poll只返回发生了事件的文件描述符的个数**，若知道是哪个发生了事件，同样需要遍历
  - epoll返回的发生了事件的**个数**和**结构体数组**，结构体包含socket的信息，因此直接处理返回的数组即可
- 工作模式：
  - select和poll都只能工作在相对低效的LT模式下
  - epoll则可以工作在ET高效模式，并且epoll还支持EPOLLONESHOT事件，**该事件能进一步减少可读、可写和异常事件被触发的次数**。
- 应用场景：
  - 当所有的fd都是活跃连接，使用epoll，需要建立文件系统，红黑书和链表对于此来说，效率反而不高，不如selece和poll
  - 当**监测的fd数目较小**，且各个fd**都比较活跃**，建议使用select或者poll
  - 当监测的fd数目非常大，成千上万，且单位时间只有其中的一部分fd处于就绪状态，这个时候使用epoll能够明显提升性能

## 7.2 HTTP报文格式

### 7.2.1 请求报文

HTTP请求报文由**请求行（request line）**、**请求头部（header）**、**空行**和**请求数据**四个部分组成

- GET

  ```C++
  GET /562f25980001b1b106000338.jpg HTTP/1.1
  Host:img.mukewang.com
  User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64)
  AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36
  Accept:image/webp,image/*,*/*;q=0.8
  Referer:http://www.imooc.com/
  Accept-Encoding:gzip, deflate, sdch
  Accept-Language:zh-CN,zh;q=0.8
  空行
  请求数据为空
  ```

- POST

  ```C++
  POST / HTTP1.1
  Host:www.wrox.com
  User-Agent:Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022)
  Content-Type:application/x-www-form-urlencoded
  Content-Length:40
  Connection: Keep-Alive
  空行
  name=Professional%20Ajax&publisher=Wiley
  ```

> - **请求行**，用来说明请求类型,要访问的资源以及所使用的HTTP版本。
>   GET说明请求类型为GET，/562f25980001b1b106000338.jpg(URL)为要访问的资源，该行的最后一部分说明使用的是HTTP1.1版本。
>
> - **请求头部**，紧接着请求行（即第一行）之后的部分，用来说明服务器要使用的附加信息。
>
> - - HOST，给出请求资源所在服务器的域名。
>   - User-Agent，HTTP客户端程序的信息，该信息由你发出请求使用的浏览器来定义,并且在每个请求中自动发送等。
>   - Accept，说明用户代理可处理的媒体类型。
>   - Accept-Encoding，说明用户代理支持的内容编码。
>   - Accept-Language，说明用户代理能够处理的自然语言集。
>   - Content-Type，说明实现主体的媒体类型。
>   - Content-Length，说明实现主体的大小。
>   - Connection，连接管理，可以是Keep-Alive或close。
>
> - **空行**，请求头部后面的空行是必须的即使第四部分的请求数据为空，也必须有空行。
>
> - **请求数据**也叫主体，可以添加任意的其他数据。

### 7.2.2 响应报文

HTTP响应也由四个部分组成，分别是：**状态行**、**消息报头**、**空行**和**响应正文**。

```C++
HTTP/1.1 200 OK
Date: Fri, 22 May 2009 06:07:21 GMT
Content-Type: text/html; charset=UTF-8
空行
<html>
      <head></head>
      <body>
            <!--body goes here-->
      </body>
</html>
```

> - **状态行**，由HTTP协议版本号， 状态码， 状态消息 三部分组成。
>   第一行为状态行，（HTTP/1.1）表明HTTP版本为1.1版本，状态码为200，状态消息为OK。
> - **消息报头**，用来说明客户端要使用的一些附加信息。
>   第二行和第三行为消息报头，Date:生成响应的日期和时间；Content-Type:指定了MIME类型的HTML(text/html),编码类型是UTF-8。
> - **空行**，消息报头后面的空行是必须的。
> - **响应正文**，服务器返回给客户端的文本信息。空行后面的html部分为响应正文。

### 7.2.3 HTTP状态码

HTTP有5种类型的状态码，具体的：

- 1xx：指示信息--表示请求已接收，继续处理。

- 2xx：成功--表示请求正常处理完毕。

- - 200 OK：客户端请求被正常处理。
  - 206 Partial content：客户端进行了范围请求。

- 3xx：重定向--要完成请求必须进行更进一步的操作。

- - 301 Moved Permanently：永久重定向，该资源已被永久移动到新位置，将来任何对该资源的访问都要使用本响应返回的若干个URI之一。
  - 302 Found：临时重定向，请求的资源现在临时从不同的URI中获得。

- 4xx：客户端错误--请求有语法错误，服务器无法处理请求。

- - 400 Bad Request：请求报文存在语法错误。
  - 403 Forbidden：请求被服务器拒绝。
  - 404 Not Found：请求不存在，服务器上找不到请求的资源。

- 5xx：服务器端错误--服务器处理请求出错。

- - 500 Internal Server Error：服务器在执行请求时出现错误。

## 7.3 服务器响应请求报文

### 7.3.1 基础API

- **stat**

  stat函数用于取得指定文件的文件属性，并将文件属性存储在结构体stat里，这里仅对其中用到的成员进行介绍

  ```C++
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <unistd.h>
  
  //获取文件属性，存储在statbuf中
  int stat(const char *pathname, struct stat *statbuf);
  
  struct stat 
  {
     mode_t    st_mode;        /* 文件类型和权限 */
     off_t     st_size;        /* 文件大小，字节数*/
  };
  ```

- **mmap**

  用于将一个文件或其他对象映射到内存，提高文件的访问速度

  ```c++
  void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
  int munmap(void* start,size_t length);
  ```

  - start：映射区的开始地址，**设置为0时表示由系统决定映射区的起始地址**

  - length：映射区的长度

  - prot：期望的内存保护标志，不能与文件的打开模式冲突

  - - PROT_READ 表示页内容可以被读取
    - PROT_WRITE

  - flags：指定映射对象的类型，映射选项和映射页是否可以共享

  - - MAP_PRIVATE 建立一个写入时拷贝的**私有映射**，**内存区域的写入不会影响到原文件**
    - MAP_SHARED

  - fd：有效的文件描述符，一般是由open()函数返回

  - off_toffset：被映射对象内容的起点

- **iovec**

  定义了一个向量元素，通常，这个结构用作一个多元素的数组

  ```C++
  struct iovec {
      void      *iov_base;      /* starting address of buffer */
      size_t    iov_len;        /* size of buffer */
  };
  ```

  - 对于每一个传输的元素，指针成员`iov_base`指向一个缓冲区，这个缓冲区是存放的是`readv`所接收的数据或是`writev`将要发送的数据。
  - 成员`iov_len`在各种情况下分别确定了接收的最大长度以及实际写入的长度。

- **writev / readv**

  writev函数用于在一次函数调用中**写多个非连续缓冲区**，有时也将这该函数称为聚集写。

  ```C++
  #include <sys/uio.h>
  ssize_t writev(int filedes, const struct iovec *iov, int iovcnt);
  ssize_t readv(int filedes, const struct iovec *iov, int iovcnt);
  ```

  - `filedes`表示文件描述符
  - `iov`为前述io向量机制结构体`iovec`
  - `iovcnt`为结构体的个数

  若成功则返回已写的字节数，若出错则返回-1。`writev`以顺序`iov[0]`，`iov[1]`至`iov[iovcnt-1]`从缓冲区中聚集输出数据。`writev`返回输出的字节总数，**通常，它应等于所有缓冲区长度之和**。

**特别注意：** 循环调用`writev`时，需要重新处理`iovec`中的指针和长度，该函数不会对这两个成员做任何处理。`writev`的返回值为已写的字节数，但**这个返回值“实用性”并不高**，因为参数传入的是iovec数组，**计量单位是iovcnt，而不是字节数**，我们仍然需要通过遍历`iovec`来计算新的基址，另外写入数据的“结束点”可能位于一个iovec的中间某个位置，因此需要调整临界`iovec`的`io_base`和`io_len`。



# 8. 线程池

- 线程池是由服务器预先创建的一组子线程，**线程池中的线程数量应该和CPU数量差不多**。线程池中的所有子线程都运行着相同的代码。当有新的任务到来时，主线程将通过某种方式选择线程池中的某一个子线程来为之服务。**相比与动态的创建子线程，选择一个已经存在的子线程的代价显然要小得多**。至于主线程选择哪个子线程来为新任务服务，则有多种方式：
  - 主线程使用某种算法来主动选择子线程：
    - 最简单、最常用的算法是**随机算法**和**Round Robin(轮流选取）算法**，但更优秀、更智能的算法将使任务在各个工作线程中更均匀地分配，从而减轻服务器的整体压力。
  - 主线程和所有子线程通过一个**共享的工作队列**来同步，子线程都睡眠在该工作队列上：
    - 当有新的任务到来时,主线程将任务添加到工作队列中。这将唤醒正在等待任务的子线程，不过只有一个子线程将获得新任务的"接管权”，它可以从工作队列中取出任务并执行之，而其他子线程将继续睡眠在工作队列上。

- 线程池一般模型：

  ![image-20220618183504722](https://cdn.jsdelivr.net/gh/WanliBaipiao/pictureGo/img/image-20220618183504722.png)

> 线程池中的线程数量最直接的限制因素是中央处理器(CPU)的处理器(processors/cores)数量N：如果你的CPU是4核的，对于**CPU密集型的任务（如视频剪辑等<font color=red>消耗CPU计算资源</font>的任务）**来说，那线程池中的线程数量最好也设置为4（**或者+1防止其他因素造成的线程阻塞**）；对于I/O密集型的任务，**一般要多于CPU的核数**，**因为线程间竞争的不是CPU的计算资源而是IO**，**<font color=red>IO的处理一般较慢</font>**，多于核心数的线程将为CPU争取更多的任务，不至在线程处理IO的过程造成CPU空闲导致资源浪费。

- 线程池特点：
  - 空间换时间，以服务器的硬件资源去换取更高的运行效率
  - 池是一组资源的集合，这组资源在服务器启动之初就被完全创建好并初始化，故被称为**静态资源**
  - 当服务器进入正式运行阶段，开始处理客户请求之时，如果需要相关资源，可直接从池中获取，**无需动态分配**
  - 当服务器处理完一个客户连接后，可以把相关资源放回池中，**无需执行系统调用释放资源**

# 9. epoll触发模式

- ET：边缘触发模式
  - 对于边缘触发模式，**在一个事件从无到有时才会触发**
  - **边缘触发模式一般和非阻塞 I/O 搭配使用**，程序会一直执行 I/O 操作，直到系统调用（如 `read` 和 `write`）返回错误，错误类型为 `EAGAIN` 或 `EWOULDBLOCK`
- LT：水平触发模式
  - 对于水平触发模式，**一个事件只要有，就会一直触发**

****

- 以`socket`的读事件为例：
  - 对于LT模式，只要在`socket`上有未读完的数据，就会一直产生`EPOLLIN`事件；
  - 对于ET模式，`socket`上每新来一次数据就会触发一次，如果上一次触发后未将`socket`上的数据读完，也不会再触发，除非再新来一次数据。必须要一次性将数据读取完，使用非阻塞I/O，读取到出现eagain
- 对于`socket`写事件：
  - 对于LT模式，如果`socket`的TCP窗口一直不饱和，就会一直触发`EPOLLOUT`事件；
  - 对于ET模式，只会触发一次，除非TCP窗口由不饱和变成饱和再一次变成不饱和，才会再次触发`EPOLLOUT`事件。



## 9.1 EPOLLONESHOT事件

-  出现的问题：
  1. 一个线程读取某个socket上的数据后开始处理数据，在处理过程中该socket上又有新数据可读，此时另一个线程被唤醒读取，此时出现两个线程处理同一个socket
  2. 即使使用ET模式，一个`socket`上的某个事件还是可能被触发多次。这在并发程序中就会引起一个问题。比如一个线程在读取完某个`socket `上的数据后开始处理这些数据，而在数据的处理过程中该`socket`上又有新数据可读（`EPOLLIN`再次被触发)，此时另外一个线程被唤醒来读取这些新的数据。于是就出现了两个线程同时操作一个`socket`的局面。一个`socket`连接在任一时刻都只被一个线程处理，可以使用`epoll` 的`EPOLLONESHOT`事件实现。
- 对于注册了`EPOLLONESHOT`事件的文件描述符，**操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次**，除非我们使用`epoll_ctl `函数重置该文件描述符上注册的`EPOLLONESHOT`事件。这样，当一个线程在处理某个`socket`时，其他线程是不可能有机会操作该`socket`的。但反过来思考，注册了`EPOLLONESHOT`事件的 `socket`一旦被某个线程处理完毕，**该线程就应该立即重置这个`socket` 上的`EPOLLONESHOT`事件**，以确保这个`socket `下一次可读时，其`EPOLLIN`事件能被触发，进而让其他工作线程有机会继续处理这个`socket`。
