//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___BLOCK_QUEUE_H
#define GAME_C___BLOCK_QUEUE_H

/*
 * 异步写入方式：将生产者-消费者模型封装为阻塞队列。将所写的日志内容先存入阻塞队列，写线程从阻塞队列中取出内容，写入日志。
 */

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "lock.h"
using namespace std;

template <class T>
class block_queue
{
public:
  // 构造函数
  block_queue(int max_size = 1000)
  {
    if (max_size <= 0)
    {
      exit(-1);
    }

    // 队列最大长度
    this->m_max_size = max_size;
    // 循环数组实现队列
    this->m_array = new T[max_size];
    // 队列长度
    this->m_size = 0;
    // 队首下标
    this->m_front = -1;
    // 队尾下标
    this->m_back = -1;
  }

  // 清空队列
  void clear()
  {
    /****** 上锁 ******/
    m_mutex.lock();

    // "逻辑上"清空
    m_size = 0;
    m_front = -1;
    m_back = -1;

    /****** 解锁 ******/
    m_mutex.unlock();
  }

  // 析构函数
  ~block_queue()
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (m_array != NULL)
      delete[] m_array;

    /****** 解锁 ******/
    m_mutex.unlock();
  }

  // 判断队列是否已满
  bool full()
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (m_size >= m_max_size)
    {
      /****** 解锁 ******/
      m_mutex.unlock();
      return true;
    }

    /****** 解锁 ******/
    m_mutex.unlock();
    return false;
  }

  // 判断队列是否为空
  bool empty()
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (0 == m_size)
    {
      /****** 解锁 ******/
      m_mutex.unlock();
      return true;
    }

    /****** 解锁 ******/
    m_mutex.unlock();
    return false;
  }

  // 返回队首元素
  bool front(T &value) // value为传出参数
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (0 == m_size)
    {
      /****** 解锁 ******/
      m_mutex.unlock();
      return false;
    }
    value = m_array[m_front];

    /****** 解锁 ******/
    m_mutex.unlock();
    return true;
  }

  // 返回队尾元素
  bool back(T &value) // value为传出参数
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (0 == m_size)
    {
      /****** 解锁 ******/
      m_mutex.unlock();
      return false;
    }
    value = m_array[m_back];

    /****** 解锁 ******/
    m_mutex.unlock();
    return true;
  }

  // 返回队列长度
  int size()
  {
    int tmp = 0;
    /****** 上锁 ******/
    m_mutex.lock();

    tmp = m_size;

    /****** 解锁 ******/
    m_mutex.unlock();
    return tmp;
  }

  // 返回队列最大长度
  int max_size()
  {
    return m_max_size;
  }

  // 往队列添加元素
  bool push(const T &item)
  {
    /****** 上锁 ******/
    m_mutex.lock();

    if (m_size >= m_max_size)
    {
      // 将所有使用队列被阻塞的线程唤醒
      m_cond.broadcast();
      /****** 解锁 ******/
      m_mutex.unlock();
      return false;
    }
    // 往队列添加元素【生产者生产了一个元素】
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;

    // 将所有使用队列被阻塞的线程唤醒
    m_cond.broadcast();

    /****** 解锁 ******/
    m_mutex.unlock();
    return true;
  }

  // 取出队头元素
  bool pop(T &item) // item为传出参数
  {
    /****** 上锁 ******/
    m_mutex.lock();

    // 等待条件变量，直到队列中有元素才跳出循环
    while (m_size <= 0)
    {
      if (!m_cond.wait(m_mutex.get()))
      {
        m_mutex.unlock();
        return false;
      }
    }
    // 取出队头元素
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;

    /****** 解锁 ******/
    m_mutex.unlock();
    return true;
  }

  // 取出队头元素
  // 增加了超时处理，将线程阻塞一定的时间长度，时间到达之后，线程就解除阻塞了
  bool pop(T &item, int ms_timeout) // item为传出参数
  {
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    // 获得此刻的时间戳
    gettimeofday(&now, NULL);

    /****** 上锁 ******/
    m_mutex.lock();

    if (m_size <= 0)
    {
      // 秒
      t.tv_sec = now.tv_sec + ms_timeout / 1000;
      // 纳秒
      t.tv_nsec = (ms_timeout % 1000) * 1000;
      if (!m_cond.timewait(m_mutex.get(), t))
      {
        m_mutex.unlock();
        return false;
      }
    }

    if (m_size <= 0)
    {
      /****** 解锁 ******/
      m_mutex.unlock();
      return false;
    }
    // 取出队头元素
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;

    /****** 解锁 ******/
    m_mutex.unlock();
    return true;
  }

private:
  // 互斥锁
  locker m_mutex;
  // 条件变量
  cond m_cond;
  // 循环数组实现队列
  T *m_array;
  // 队列长度
  int m_size;
  // 队列最大长度
  int m_max_size;
  // 队头下标
  int m_front;
  // 队尾下标
  int m_back;
};



#endif //GAME_C___BLOCK_QUEUE_H
