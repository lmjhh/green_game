//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___WEB_SERVER_H
#define GAME_C___WEB_SERVER_H

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "thread_pool.h"
#include "http_conn.h"

// 最大文件描述符
const int MAX_FD = 65536;
// 最大事件数
const int MAX_EVENT_NUMBER = 10000;
// 最小超时单位
const int TIMESLOT = 5;

class WebServer
{
public:
  // 构造函数
  WebServer();
  // 析构函数
  ~WebServer();
  // 初始化部分成员属性
  void init(int port, string user, string passWord, string databaseName,
            int log_write, int opt_linger, int trigmode, int sql_num,
            int thread_num, int close_log, int actor_model);

  void thread_pool();
  void sql_pool();
  void log_write();
  // 设置监听与通信文件描述符触发模式
  void trig_mode();
  void eventListen();
  void eventLoop();
  void timer(int connfd, struct sockaddr_in client_address);
  void adjust_timer(util_timer *timer);
  void deal_timer(util_timer *timer, int sockfd);
  bool dealclinetdata();
  bool dealwithsignal(bool &timeout, bool &stop_server);
  void dealwithread(int sockfd);
  void dealwithwrite(int sockfd);

public:
  /******基础******/
  // 服务器端口
  int m_port;
  // root文件夹的路径
  char *m_root;
  // 日志写入方式
  int m_log_write;
  // 是否关闭日志
  int m_close_log;
  // 并发模型选择
  int m_actormodel;

  int m_pipefd[2];
  int m_epollfd;
  // HTTP对象数组指针
  http_conn *users;

  /******数据库相关******/
  // 数据库连接池对象
  connection_pool *m_connPool;
  // 登录数据库用户名
  string m_user;
  // 登录数据库密码
  string m_passWord;
  // 使用数据库名
  string m_databaseName;
  // 数据库连接池数量
  int m_sql_num;

  /******线程池相关******/
  // 线程池对象
  threadpool<http_conn> *m_pool;
  // 线程池内的线程数量
  int m_thread_num;

  /******epoll_event相关******/
  // 结构体数组，存储已就绪的文件描述符的信息
  epoll_event events[MAX_EVENT_NUMBER];
  // 监听文件描述符
  int m_listenfd;
  // 是否优雅关闭连接
  int m_OPT_LINGER;
  // 触发组合模式
  int m_TRIGMode;
  // 监听文件描述符触发方式
  int m_LISTENTrigmode;
  // 通信文件描述符触发方式
  int m_CONNTrigmode;

  /******定时器相关******/
  // 连接资源对象数组指针
  client_data *users_timer;
  Utils utils;
};
#endif



#endif //GAME_C___WEB_SERVER_H
