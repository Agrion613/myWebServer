#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;

// 初始化数据库连接
void http_conn::initmysql_result(connection_pool *connPool){

    // 创建一个空的MYSQL对象
    MYSQL *mysql = NULL;
    // 通过RAII机制从连接池connPool中取出一个连接（即初始化上一行代码中的mysql）
    connectionRAII mysqlcon(&mysql, connPool);

    // 在user表中检索username和passwd数据
    // 后面字符串为MYSQL语句
    // 注意不管是否查询到信息，该函数都返回0，如果返回值非0，代表SELECT语句出现了错误
    if(mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT ERROR:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码依次存入map中
    while(MYSQL_ROW row = mysql_fetch_row(result)){
        string tmp1(row[0]);
        string tmp2(row[1]);
        users[tmp1] = tmp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMode){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 将内核事件表上的描述符删除
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLPNONESHOT
// 原因见笔记中的9.1
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMode){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close){
    if(real_close && (m_epollfd != -1)){
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}


//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}


// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
// 在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n。
// 因此，可以通过查找\r\n将报文拆解成单独的行进行解析，项目中便是利用了这一点
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    // 逐字节读取
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];

        // 读到了\r则判断下一位是否为\n
        if (temp == '\r')
        {
            // 当前已经在buffer的末尾，则表示buffer还需要继续接收，返回LINE_OPEN
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            // 当前读取的字符不在末尾，且下一位为\n
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                // 将\r\n变为\0\0，并移动到\0\0的后面（即移到了下一行）
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                // 读取成功则返回LINE_OK，表示完整读取了一行
                return LINE_OK;
            }
            // 其他情况代表HTTP报文有语法错误，返回LINE_BAD
            return LINE_BAD;
        }

        // 如果当前为\n，则需要判断上一个是否为\r
        else if (temp == '\n')
        {
            // 当前读取的字符不在第一位，且上一位为\r，则代表完整读取了一行
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    // 如果能运行到此代表buffer还未接收完整（即未找到一行结尾的标志\r\n），需要继续接收
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    // LT读取数据
    // 只读一次，一次最大可读缓存剩余长度的数据
    if (0 == m_TRIGMode)
    {
        // 第二个参数表示读取到的数据从哪里开始存储（注意是指针）
        // 第三个参数表示本次读取的数据，不能超过READ_BUFFER_SIZE - m_read_idx（缓存区中还剩的最大容量）
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

        // 实时记录缓存区的大小
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    // ET读数据
    else
    {
        // ET模式要一次性将数据读完，所以使用while循环直到数据读完
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                // 此时代表socket上的数据已被读完，则跳出while循环
                // EAGAIN和EWOULDBLOCK等效
                // 在recv读取非阻塞文件描述符的情况下，这不算错误，代表此时已无数据可读
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}


// 解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // 在text中找到第一个存在于字符串中的字符位置
    // 也即在text中找到第一个出现空格字符或者\t字符的位置
    // 并返回从这个位置开始的剩余字符
    m_url = strpbrk(text, " \t");

    // 没有找到，则返回空
    // 代表请求中的报文语法有错误
    if (!m_url)
    {
        return BAD_REQUEST;
    }

    // 将空格字符或者\t字符改为\0
    // 是为了提取出请求方法
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    
    // 返回第一个没有出现字符串中字符的位置
    // 也就是从m_url开始找，直到没有出现空格字符或\t字符为止
    // 返回经过的字符数量
    // 此处就是为了跳过多余的空格字符或者\t字符
    m_url += strspn(m_url, " \t");

    // 获取版本号
    // 流程同上，首先通过strpbk找到第一个空格字符或\t字符
    // 然后通过strspn跳过多余的空格字符或\t字符
    // 最后获取的字符串即所需字符
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    // 这里只接受HTTP/1.1版本
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    // 如果url前有http://，则需要跳过
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        // 找到第一次出现所需字符的位置，并返回以其开头的剩余字符串
        // 比如strchr("./123", '/')，则返回"/123"
        // 这里其实是为了找到主网址后面的内容
        m_url = strchr(m_url, '/');
    }
    // 同上
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    // 这里表示url的格式不正确，直接返回BAD_REQUEST
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    
    // 当url为/时，就代表所请求的页面就是主网址
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    
    // 解析完请求行后，需要解析请求头，所以下一个状态为解析请求头
    m_check_state = CHECK_STATE_HEADER;

    // 由于还需要解析请求头，所以需要继续请求报文内容
    return NO_REQUEST;
}


// 解析http请求的一个头部信息（解析请求头）
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    // 读到了空行
    // 需要判断是否有请求数据（即判断是GET还是POST）
    if (text[0] == '\0')
    {
        // 有请求数据，为POST
        if (m_content_length != 0)
        {
            // 继续进行下一个状态，并返回NO_REQUEST，表示需要继续读取请求报文数据
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 无请求数据，为GET，则此时GET请求已经解析完毕，返回GET_REQUEST
        return GET_REQUEST;
    }


    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
// 即解析POST请求中的最后一部分：请求数据
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 状态机：状态转移流程
http_conn::HTTP_CODE http_conn::process_read(){
    // 从状态机的初始状态为完整读取一行成功
    LINE_STATUS line_status = LINE_OK;
    // 报文解析的初始状态为：需要继续读取报文数据
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    // 继续进入循环的规则：
    // 主状态机的状态是解析消息体（POST请求）且从状态机的状态是完整读取了一行
    // 或者
    // 从状态机完整读取了一行，这时也需要继续判断主状态机的状态，因为读取的这一行可能是
    // 需要解析的行
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);

        // 主状态机
        switch (m_check_state)
        {
        // 主状态机状态为解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            // 解析错误则直接返回，跳出本次解析
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        }

        // 解析请求头
        case CHECK_STATE_HEADER:
        {  
            ret = parse_headers(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }else if(ret == GET_REQUEST){
                // 此时代表GET请求解析成功
                // 则通过调用do_request()
                // 给客户返回所请求的资源
                return do_request();
            }
            break;
        }

        // 解析请求数据（POST请求）
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            // parse_content()只会返回两种状态
            if(ret == GET_REQUEST){
                return do_request();
            }
            // 第二种状态代表还需要继续读
            // 所以设定从状态机状态为LINE_OPEN
            line_status = LINE_OPEN;
            break;
        }
        // 服务器内部错误，一般不触发
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    // 将初始化的m_real_file赋值为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);

    // 找到m_url中最后一个'/'出现的位置
    // 目的是为了找到'/'后面的数字是多少
    // 用来判断下一步操作
    // 如果只有'/'则代表为GET请求，如果是/0,/1等，则是POST请求
    const char *p = strrchr(m_url, '/');

    // 处理cgi
    // /2和/3的情况
    // 分别代表登录校验和注册校验
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来
        // 其格式为：
        // user=123&password=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            // 首先将sql的插入语句拼接好
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            // 重复用户名名检测
            // 如果没有找到重复的，则插入
            if (users.find(name) == users.end())
            {
                // 操作数据库需要上锁
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                // 录入数据库后，users也要更新
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                // SQL语句无错误，则注册成功
                // 下一步进入登录页面
                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    // 否则，进入注册错误页面
                    strcpy(m_url, "/registerError.html");
            }
            // 有重复的用户名，进入注册错误页面
            else
                strcpy(m_url, "/registerError.html");
        }

        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }


    // /0表示需要跳转到注册页面
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // /1表示跳转到登录界面
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // /5表示跳转到图片请求界面
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // /6表示跳转到视频请求界面
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // /7表示跳转到关注界面
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/aboutme.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        // 如果以上均不符合，则直接将url与网站目录拼接并返回
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // 通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    // 失败则返回NO_RESOURCE表示资源不存在
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    // 判断文件的权限是否为可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 判断文件类型，如果是目录则返回BAD_REQUEST，表示请求有误
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // 已只读的方式打开文件描述符，通过mmap将文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    close(fd);

    // 表示请求文件存在，且可以访问
    return FILE_REQUEST;
}

// 解除mmap映射
void http_conn::unmap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 服务器子线程调用process_write完成响应报文，随后注册epollout事件。
// 服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端
bool http_conn::write()
{
    int temp = 0;

    int newadd = 0;

    // 若要发送的数据长度为0
    // 表示响应报文为空，一般不会出现这种情况
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        // 将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);

        // 正常发送，temp为发送的字节数
        if(temp > 0){
            // 更新已经发送的字节数
            bytes_have_send += temp;
            // 偏移文件iovec的指针
            newadd = bytes_have_send - m_write_idx;
        }

        // 返回值不正确，未正常发送
        if (temp < 0)
        {
            // 情况一：缓冲区已满
            if (errno == EAGAIN)
            {
                // 第一个iovec头部信息的数据已经发完，发送第二个iovec数据
                if(bytes_have_send >= m_iv[0].iov_len){
                    // 不再继续发送头部信息
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }
                // 继续发送第一个iovec头部信息的数据
                else{
                    m_iv[0].iov_base = m_write_buf + bytes_to_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                }
                // 重新注册写事件
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }

            // 其他情况：发送失败但不是缓冲区问题，此时取消映射
            unmap();
            return false;
        }

        // 更新已发送字节数
        bytes_to_send -= temp;
        
        // 数据已经全部发送完毕
        if (bytes_to_send <= 0)
        {
            unmap();

            // 在epoll树上重置EPOLLONESHOT事件
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
            
            // 浏览器的请求为长连接
            if (m_linger)
            {
                // 重新初始化HTTP对象
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}


/************************ 以下为添加响应报文 ************************/
bool http_conn::add_response(const char *format, ...)
{
    // 如果写入内容超出buffer的最大空间则报错
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    // 定义可变参数列表
    va_list arg_list;

    // 将变量arg_list初始化为传入参数
    va_start(arg_list, format);

    // 将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    
    // 如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    // 更新m_write_idx位置
    m_write_idx += len;

    // 清空可变参列表
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

// 添加状态行
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 添加消息报头：添加的文本长度、连接状态和空行
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}

// 添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

// 添加文本类型，这里是html
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

// 添加文本content
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
/****************************************************************************/

// 服务器子线程调用process_write向m_write_buf中写入响应报文
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    // 内部错误，500
    case INTERNAL_ERROR:
    {
        // 状态行
        add_status_line(500, error_500_title);
        // 消息报头
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    // 报文语法有误，404
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    // 资源没有访问权限，403
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    // 文件存在，200
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        // 如果请求的资源存在
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            // 第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            // 第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            // 发送的全部数据为响应报文头部信息和文件大小
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            // 如果请求的资源大小为0，则返回空白html文件
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    // 因为只有FILE_REQUEST状态表示请求资源可访问，所以在这个状态下，才需要访问资源文件
    // 所以在这种情况才使用mmap将一个文件映射到内存，提高文件的访问速度
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}


void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        // 如果返回NO_REQUEST，代表还需要读取数据
        // 所以重新注册EPOLLONESHOT事件
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    // 根据读取报文后的操作（返回标志），进行写操作
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        // 返回值错误，则关闭连接
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
