//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___THREAD_POOL_H
#define GAME_C___THREAD_POOL_H

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "lock.h"
#include "sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
  // 构造函数
  threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
  // 析构函数
  ~threadpool();

  // 向任务队列中插入任务
  bool append(T *request, int state);
  bool append_p(T *request);

private:
  // 工作线程运行的函数，它不断从任务队列中取出任务并执行之
  static void *worker(void *arg);
  void run();

private:
  // 线程池中的线程数
  int m_thread_number;
  // 任务队列中允许的最大任务数
  int m_max_requests;
  // 描述线程池的数组
  pthread_t *m_threads;
  // 任务队列
  std::list<T *> m_workqueue;
  // 保护任务队列的互斥锁
  locker m_queuelocker;
  // 是否有任务需要处理的信号量
  sem m_queuestat;
  // 数据库连接池指针
  connection_pool *m_connPool;
  // 并发模型选择
  int m_actor_model;
};

// 构造函数
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool)
{
  if (thread_number <= 0 || max_requests <= 0)
    throw std::exception();
  // 线程池初始化
  this->m_threads = new pthread_t[this->m_thread_number];
  if (!this->m_threads)
    throw std::exception();
  // 循环创建线程，同时设置其回调函数worker
  for (int i = 0; i < thread_number; ++i)
  {
    if (pthread_create(this->m_threads + i, NULL, worker, this) != 0)
    {
      delete[] this->m_threads;
      throw std::exception();
    }
    // 线程分离，之后不用单独对工作线程进行回收
    if (pthread_detach(this->m_threads[i]))
    {
      delete[] this->m_threads;
      throw std::exception();
    }
  }
}

// 析构函数
template <typename T>
threadpool<T>::~threadpool()
{
  delete[] m_threads;
}

// 向任务队列中插入任务
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
  /****** 加锁 ******/
  m_queuelocker.lock();

  // 任务队列中任务数 >= 任务队列中允许的最大请求数
  if (m_workqueue.size() >= m_max_requests)
  {
    /****** 解锁 ******/
    m_queuelocker.unlock();
    return false;
  }

  // 0：读; 1：写
  request->m_state = state;

  // 添加任务到任务队列
  m_workqueue.push_back(request);

  /****** 解锁 ******/
  m_queuelocker.unlock();

  // 信号量+1，提醒有任务要处理
  m_queuestat.post();

  return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
  /****** 加锁 ******/
  m_queuelocker.lock();
  // 任务队列中任务数 >= 任务队列中允许的最大任务数
  if (m_workqueue.size() >= m_max_requests)
  {
    /****** 解锁 ******/
    m_queuelocker.unlock();
    return false;
  }
  // 添加任务到任务队列
  m_workqueue.push_back(request);

  /****** 解锁 ******/
  m_queuelocker.unlock();

  // 信号量+1，提醒有任务要处理
  m_queuestat.post();

  return true;
}

// 工作线程运行的函数
template <typename T>
void *threadpool<T>::worker(void *arg)
{
  // 将参数强转为线程池类类型
  threadpool *pool = (threadpool *)arg;
  // 调用run成员方法
  pool->run();
  return pool;
}

template <typename T>
void threadpool<T>::run()
{
  while (true)
  {
    // 等待信号量
    m_queuestat.wait();

    /****** 加锁 ******/
    m_queuelocker.lock();

    // 请求队列为空
    if (m_workqueue.empty())
    {
      /****** 解锁 ******/
      m_queuelocker.unlock();
      continue;
    }
    // 从任务队列中取出第一个任务
    T *request = m_workqueue.front();
    // 将任务从任务队列删除
    m_workqueue.pop_front();

    /****** 解锁 ******/
    m_queuelocker.unlock();

    if (!request)
      continue;

    // Reactor模型
    // 主线程只负责监听文件描述符上是否有事件发生，有的话立即通知工作线程。读写数据等处理逻辑均在工作线程中完成。
    if (1 == m_actor_model)
    {
      // 读
      if (0 == request->m_state)
      {
        // 将客户端请求报文读到读缓冲区
        if (request->read_once())
        {
          request->improv = 1;
          // 从数据库连接池拿出一个连接
          connectionRAII mysqlcon(&request->mysql, m_connPool);
          // 整个业务逻辑
          request->process();
        }
        else
        {
          request->improv = 1;
          request->timer_flag = 1;
        }
      }
        // 写
      else
      {
        // 将响应报文从写缓冲区写出
        if (request->write())
        {
          request->improv = 1;
        }
        else
        {
          request->improv = 1;
          request->timer_flag = 1;
        }
      }
    }
      // Proactor模型
      // 主线程和内核负责处理读写数据、接受新连接等I/O操作，工作线程仅负责业务逻辑。
    else
    {
      // 从数据库连接池拿出一个连接
      connectionRAII mysqlcon(&request->mysql, m_connPool);
      // 整个业务逻辑
      request->process();
    }
  }
}


#endif //GAME_C___THREAD_POOL_H
