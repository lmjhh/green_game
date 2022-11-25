//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___SQL_CONNECTION_POOL_H
#define GAME_C___SQL_CONNECTION_POOL_H

// sql_connection_pool.h
#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
  // 获取数据库连接
  MYSQL *GetConnection();
  // 释放连接
  bool ReleaseConnection(MYSQL *conn);
  // 获取空闲连接数
  int GetFreeConn();
  // 销毁所有连接
  void DestroyPool();

  // 懒汉式单例模式
  static connection_pool *GetInstance();

  // 初始化
  void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
  // 构造函数
  connection_pool();
  // 析构函数
  ~connection_pool();
  // 最大连接数
  int m_MaxConn;
  // 当前已使用的连接数
  int m_CurConn;
  // 当前空闲的连接数
  int m_FreeConn;
  // 连接池
  list<MYSQL *> connList;
  // 互斥锁
  locker lock;
  // 信号量
  sem reserve;

public:
  // 数据库主机地址
  string m_url;
  // 数据库端口号
  string m_Port;
  // 登陆数据库用户名
  string m_User;
  // 登陆数据库密码
  string m_PassWord;
  // 使用的数据库名
  string m_DatabaseName;
  // 日志是否关闭
  int m_close_log;
};

class connectionRAII
{
public:
  // 通过二重指针对MYSQL *con修改
  connectionRAII(MYSQL **con, connection_pool *connPool);
  ~connectionRAII();

private:
  MYSQL *conRAII;
  connection_pool *poolRAII;
};


#endif //GAME_C___SQL_CONNECTION_POOL_H
