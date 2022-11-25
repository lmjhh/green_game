//
// Created by lianyu on 2022/11/25.
//

#include "include/sql_connection_pool.h"

#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "include/sql_connection_pool.h"

using namespace std;

// 构造函数
connection_pool::connection_pool()
{
  // 当前已使用的连接数
  m_CurConn = 0;
  // 当前空闲的连接数
  m_FreeConn = 0;
}

// 懒汉式单例模式
connection_pool *connection_pool::GetInstance()
{
  static connection_pool connPool;
  return &connPool;
}

// 初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
  // 初始化数据库信息
  // 数据库主机地址
  m_url = url;
  // 数据库端口号
  m_Port = Port;
  // 登陆数据库用户名
  m_User = User;
  // 登陆数据库密码
  m_PassWord = PassWord;
  // 使用数据库名
  m_DatabaseName = DBName;
  // 日志是否关闭
  m_close_log = close_log;

  // 创建MaxConn条数据库连接
  for (int i = 0; i < MaxConn; i++)
  {
    // 初始化连接环境
    MYSQL *connect = NULL;
    connect = mysql_init(connect);
    if (connect == NULL)
    {
      LOG_ERROR("MySQL Error");
      exit(1);
    }

    // 连接MySQL服务器
    connect = mysql_real_connect(connect, url.c_str(), User.c_str(), PassWord.c_str(),
                                 DBName.c_str(), Port, NULL, 0);
    if (connect == NULL)
    {
      LOG_ERROR("MySQL Error");
      exit(1);
    }

    // 该连接加入数据库连接池
    connList.push_back(connect);
    // 当前空闲的连接数+1
    m_FreeConn++;
  }

  // 将信号量初始化为最大连接数
  reserve = sem(m_FreeConn);
  // 设置最大连接数
  m_MaxConn = m_FreeConn;
}

// 从数据库连接池中返回一个可用连接
MYSQL *connection_pool::GetConnection()
{
  MYSQL *connect = NULL;

  if (0 == connList.size())
    return NULL;

  // 取出连接，以原子操作方式将信号量-1，信号量为0时阻塞。
  reserve.wait();

  /****** 加锁 ******/
  lock.lock();

  // 连接池中取出一个连接
  connect = connList.front();
  connList.pop_front();
  // 当前空闲的连接数-1
  m_FreeConn--;
  // 当前已使用的连接数+1
  m_CurConn++;

  /****** 解锁 ******/
  lock.unlock();

  return connect;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *connect)
{
  if (NULL == connect)
    return false;

  /****** 加锁 ******/
  lock.lock();

  // 连接池中放回一个连接
  connList.push_back(connect);
  // 当前空闲的连接数+1
  ++m_FreeConn;
  // 当前已使用的连接数-1
  --m_CurConn;

  /****** 解锁 ******/
  lock.unlock();

  // 以原子操作方式将释放连接信号量+1
  reserve.post();

  return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool()
{
  /****** 加锁 ******/
  lock.lock();
  // 通过迭代器遍历，关闭数据库连接
  if (connList.size() > 0)
  {
    list<MYSQL *>::iterator it;
    for (it = connList.begin(); it != connList.end(); ++it)
    {
      MYSQL *connect = *it;
      // 关闭MySQL实例
      mysql_close(connect);
    }
    // 当前已使用的连接数 = 0
    m_CurConn = 0;
    // 当前空闲的连接数 = 0
    m_FreeConn = 0;
    // 清空数据库连接池
    connList.clear();
  }

  /****** 解锁 ******/
  lock.unlock();
}

// 获取当前空闲的连接数
int connection_pool::GetFreeConn()
{
  return this->m_FreeConn;
}

// 析构函数
connection_pool::~connection_pool()
{
  DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) // MYSQL **SQL为传出参数
{
  // 从数据库连接池拿出一个连接
  *SQL = connPool->GetConnection();

  conRAII = *SQL;
  poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
  // 将此连接放回数据库连接池
  poolRAII->ReleaseConnection(conRAII);
}

