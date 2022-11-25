//
// Created by lianyu on 2022/11/25.
//

#include "include/web_server.h"

// 构造函数
WebServer::WebServer()
{
  // 创建MAX_FD个HTTP对象
  this->users = new http_conn[MAX_FD];

  // 当前工作目录的绝对路径
  char server_path[200];
  getcwd(server_path, 200);
  char root[6] = "/root";
  // 网站资源文件夹的路径
  this->m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
  strcpy(this->m_root, server_path);
  strcat(this->m_root, root);

  // 创建MAX_FD个连接资源对象
  this->users_timer = new client_data[MAX_FD];
}

// 析构函数
WebServer::~WebServer()
{
  close(m_epollfd);
  close(m_listenfd);
  close(m_pipefd[1]);
  close(m_pipefd[0]);
  delete[] users;
  delete[] users_timer;
  delete m_pool;
}

// 初始化
void WebServer::init(int port, string user, string passWord, string databaseName, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
  // 服务器端口
  this->m_port = port;
  // 登录数据库用户名
  this->m_user = user;
  // 登录数据库密码
  this->m_passWord = passWord;
  // 使用数据库名
  this->m_databaseName = databaseName;
  // 日志写入方式
  this->m_log_write = log_write;
  // 是否优雅关闭连接
  this->m_OPT_LINGER = opt_linger;
  // 触发组合模式
  this->m_TRIGMode = trigmode;
  // 数据库连接池数量
  this->m_sql_num = sql_num;
  // 线程池内的线程数量
  this->m_thread_num = thread_num;
  // 是否关闭日志
  this->m_close_log = close_log;
  // 并发模型选择
  this->m_actormodel = actor_model;
}

// 设置监听与通信文件描述符触发模式
void WebServer::trig_mode()
{
  // LISTENTrigmode CONNTrigmode
  // LT + LT
  if (0 == m_TRIGMode)
  {
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 0;
  }
    // LT + ET
  else if (1 == m_TRIGMode)
  {
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 1;
  }
    // ET + LT
  else if (2 == m_TRIGMode)
  {
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 0;
  }
    // ET + ET
  else if (3 == m_TRIGMode)
  {
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 1;
  }
}

// 初始化日志
void WebServer::log_write()
{
  if (0 == m_close_log)
  {
    // 异步写入方式
    if (1 == m_log_write)
    {
      Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
    }
      // 同步写入方式
    else
    {
      Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
  }
}

// 初始化数据库连接池
void WebServer::sql_pool()
{
  m_connPool = connection_pool::GetInstance();
  m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
  // 初始化数据库读取表
  users->initmysql_result(m_connPool);
}

// 初始化线程池
void WebServer::thread_pool()
{
  m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen()
{
  // 创建监听Socket文件描述符
  m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(m_listenfd >= 0);

  // 非优雅关闭连接：向对端发送一个RST报文直接进入CLOSED状态。
  if (0 == m_OPT_LINGER)
  {
    struct linger tmp = {0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }
    // 优雅关闭连接：正常的"TCP四次挥手"。
  else if (1 == m_OPT_LINGER)
  {
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }

  int ret = 0;
  // 创建监听Socket的TCP/IP的IPv4 Socket地址
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY：将套接字绑定到所有可用的接口
  address.sin_port = htons(m_port);

  // 设置端口复用
  int flag = 1;
  setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  // 绑定Socket和它的地址
  ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
  assert(ret >= 0);
  // 创建监听队列以存放待处理的客户连接
  ret = listen(m_listenfd, 5);
  assert(ret >= 0);

  // 设置超时时间
  utils.init(TIMESLOT);

  // 用于存储EPOLL事件表中就绪事件的数组
  epoll_event events[MAX_EVENT_NUMBER];
  // 创建一个额外的文件描述符来唯一标识内核中的EPOLL事件表
  m_epollfd = epoll_create(5);
  assert(m_epollfd != -1);
  // 将listenfd在内核事件表注册读事件
  utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
  // 初始化HTTP类对象的m_epollfd属性
  http_conn::m_epollfd = m_epollfd;

  // 创建管道
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
  assert(ret != -1);
  // 管道写端设置为非阻塞
  // 如果缓冲区满了则会阻塞，会进一步增加信号处理函数的执行时间
  utils.setnonblocking(m_pipefd[1]);
  // 管道读端在内核事件表注册读事件
  utils.addfd(m_epollfd, m_pipefd[0], false, 0);

  // 解决"对端关闭"问题
  utils.addsig(SIGPIPE, SIG_IGN);
  // 设置捕捉定时器超时信号
  utils.addsig(SIGALRM, utils.sig_handler, false);
  // 设置捕捉程序结束信号（kill命令 或 Ctrl+C）
  utils.addsig(SIGTERM, utils.sig_handler, false);

  // 单次定时
  alarm(TIMESLOT);

  // 初始化工具类
  Utils::u_pipefd = m_pipefd;
  Utils::u_epollfd = m_epollfd;
}

// 初始化HTTP连接对象和其定时器
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
  // 初始化该HTTP连接对象
  users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

  // ·客户端地址信息
  users_timer[connfd].address = client_address;
  // ·客户端连接的文件描述符
  users_timer[connfd].sockfd = connfd;

  // 创建定时器
  util_timer *timer = new util_timer;
  // `绑定客户端信息
  timer->user_data = &users_timer[connfd];
  // `设置回调函数
  timer->cb_func = cb_func;
  // 获得此刻的时间戳
  time_t cur = time(NULL);
  // `设置超时时间
  timer->expire = cur + 3 * TIMESLOT;

  // ·客户端的定时器
  users_timer[connfd].timer = timer;

  // 将定时器添加到双向链表中
  utils.m_timer_lst.add_timer(timer);
}

// 若该文件描述符有数据传输
void WebServer::adjust_timer(util_timer *timer)
{
  time_t cur = time(NULL);
  // 将定时器往后延迟3个单位
  timer->expire = cur + 3 * TIMESLOT;
  // 并对新的定时器在链表上的位置进行调整
  utils.m_timer_lst.adjust_timer(timer);

  LOG_INFO("%s", "Delay this timer's timeout");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
  // 删除非活动连接在Socket上的注册事件
  timer->cb_func(&users_timer[sockfd]);
  if (timer)
  {
    // 删除定时器
    utils.m_timer_lst.del_timer(timer);
  }

  LOG_INFO("Close clientFd %d", users_timer[sockfd].sockfd);
}

// 处理客户端的连接请求
bool WebServer::dealclinetdata()
{
  struct sockaddr_in client_address;
  socklen_t client_addrlength = sizeof(client_address);

  // 水平触发方式
  if (0 == m_LISTENTrigmode)
  {
    // 建立连接
    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0)
    {
      LOG_ERROR("%s:errno is:%d", "accept error", errno);
      return false;
    }
    // 连接的客户数量超过最大值
    if (http_conn::m_user_count >= MAX_FD)
    {
      // 向客户端发送错误信息
      utils.show_error(connfd, "Internal server busy");
      LOG_ERROR("%s", "Internal server busy");
      return false;
    }
    // 初始化HTTP连接和其定时器
    timer(connfd, client_address);
  }
    // 边沿触发方式
  else
  {
    while (1)
    {
      // 建立连接
      int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
      if (connfd < 0)
      {
        LOG_ERROR("%s:errno is:%d", "accept error", errno);
        break;
      }
      // 连接的客户数量超过最大值
      if (http_conn::m_user_count >= MAX_FD)
      {
        // 向客户端发送错误信息
        utils.show_error(connfd, "Internal server busy");
        LOG_ERROR("%s", "Internal server busy");
        break;
      }
      // 初始化HTTP连接和其定时器
      timer(connfd, client_address);
    }
    return false;
  }
  return true;
}

// 处理管道读事件
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
  int ret = 0;
  int sig;
  char signals[1024];
  // 从管道读端读
  ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
  if (ret == -1)
  {
    return false;
  }
  else if (ret == 0)
  {
    return false;
  }
  else
  {
    for (int i = 0; i < ret; ++i)
    {
      switch (signals[i])
      {
        // 定时器超时信号
        case SIGALRM:
        {
          // 超时标志位
          timeout = true;
          break;
        }
          // 程序结束信号
        case SIGTERM:
        {
          // 程序结束标志位
          stop_server = true;
          break;
        }
      }
    }
  }
  return true;
}

void WebServer::dealwithread(int sockfd)
{
  util_timer *timer = users_timer[sockfd].timer;

  // Reactor模式
  // 主线程只负责监听文件描述符上是否有事件发生，有的话立即通知工作线程，读写数据等处理逻辑均在工作线程中完成。
  if (1 == m_actormodel)
  {
    if (timer)
    {
      // 该文件描述符有数据传输，调整定时器
      adjust_timer(timer);
    }

    // 监测到读事件，将该事件放入线程池请求队列
    m_pool->append(users + sockfd, 0);

    while (true)
    {
      // improv：尝试过读了
      // timer_flag：读失败了
      if (1 == users[sockfd].improv)
      {
        if (1 == users[sockfd].timer_flag)
        {
          // 删除Socket上的注册事件，删除定时器
          deal_timer(timer, sockfd);
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  }
    // Proactor模式
    // 主线程和内核负责处理读写数据、接受新连接等I/O操作，工作线程仅负责业务逻辑。
  else
  {
    // 读取浏览器端发来的全部数据
    if (users[sockfd].read_once())
    {
      LOG_INFO("Deal with data from client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

      // 监测到读事件，将该事件放入线程池请求队列
      m_pool->append_p(users + sockfd);

      if (timer)
      {
        // 该文件描述符有数据传输，调整定时器
        adjust_timer(timer);
      }
    }
    else
    {
      // 删除Socket上的注册事件，删除定时器
      deal_timer(timer, sockfd);
    }
  }
}

void WebServer::dealwithwrite(int sockfd)
{
  util_timer *timer = users_timer[sockfd].timer;
  // Reactor模式
  // 主线程只负责监听文件描述符上是否有事件发生，有的话立即通知工作线程，读写数据等处理逻辑均在工作线程中完成。
  if (1 == m_actormodel)
  {
    if (timer)
    {
      // 该文件描述符有数据传输，调整定时器
      adjust_timer(timer);
    }
    // 监测到读事件，将该事件放入线程池请求队列
    m_pool->append(users + sockfd, 1);

    while (true)
    {
      // improv：尝试过写了
      // timer_flag：写失败了
      if (1 == users[sockfd].improv)
      {
        if (1 == users[sockfd].timer_flag)
        {
          // 删除Socket上的注册事件，删除定时器
          deal_timer(timer, sockfd);
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  }
  else
  {
    // Proactor模式
    // 主线程和内核负责处理读写数据、接受新连接等I/O操作，工作线程仅负责业务逻辑。
    // 将响应报文从写缓冲区写出
    if (users[sockfd].write())
    {
      LOG_INFO("Send data to client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

      if (timer)
      {
        // 该文件描述符有数据传输，调整定时器
        adjust_timer(timer);
      }
    }
    else
    {
      // 删除Socket上的注册事件，删除定时器
      deal_timer(timer, sockfd);
    }
  }
}

void WebServer::eventLoop()
{
  bool timeout = false;
  bool stop_server = false;

  while (!stop_server)
  {
    // 等待所监控文件描述符上有事件产生
    int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if (number < 0 && errno != EINTR)
    {
      LOG_ERROR("%s", "epoll failure");
      break;
    }
    // 对所有就绪事件进行处理
    for (int i = 0; i < number; i++)
    {
      int sockfd = events[i].data.fd;

      // 用于监听的文件描述符
      if (sockfd == m_listenfd)
      {
        bool flag = dealclinetdata();
        if (false == flag)
          continue;
      }
        // 处理异常事件
      else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
      {
        // 服务器端关闭连接
        util_timer *timer = users_timer[sockfd].timer;
        // 删除对应的定时器
        deal_timer(timer, sockfd);
      }
        // 处理管道中信号
      else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
      {
        bool flag = dealwithsignal(timeout, stop_server);
        if (false == flag)
          LOG_ERROR("%s", "dealclientdata failure");
      }
        // 处理客户连接上的读事件
      else if (events[i].events & EPOLLIN)
      {
        dealwithread(sockfd);
      }
        // 处理客户连接上的写事件
      else if (events[i].events & EPOLLOUT)
      {
        dealwithwrite(sockfd);
      }
    }
    // 捕捉到超时信号
    if (timeout)
    {
      // 超时信号处理，并重新定时以不断触发SIGALRM信号
      utils.timer_handler();

      LOG_INFO("%s", "Connection timeout");

      // 重新设置标志位
      timeout = false;
    }
  }
}
