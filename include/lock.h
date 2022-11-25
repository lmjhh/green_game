//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___LOCK_H
#define GAME_C___LOCK_H

#include <pthread.h>
#include <exception>

// 互斥锁
class locker
{
public:
  // 初始化互斥锁
  locker()
  {
    if (pthread_mutex_init(&m_mutex, NULL) != 0)
    {
      throw std::exception();
    }
  }
  // 释放互斥锁资源
  ~locker()
  {
    pthread_mutex_destroy(&m_mutex);
  }
  // 加锁
  // 若锁没有被锁定，此线程加锁成功，锁中记录该线程加锁成功。
  // 若锁被锁定，其他线程会加锁失败，都会阻塞在这把锁上。
  // 锁被解开后，阻塞在锁上的线程就解除阻塞，并且通过竞争的方式对锁进行加锁，没抢到锁的线程继续阻塞。
  bool lock()
  {
    return pthread_mutex_lock(&m_mutex) == 0;
  }
  // 解锁
  // 哪个线程加的锁，哪个线程才能解锁！
  bool unlock()
  {
    return pthread_mutex_unlock(&m_mutex) == 0;
  }
  // 获取互斥锁地址
  pthread_mutex_t *get()
  {
    return &m_mutex;
  }

private:
  // 互斥锁
  pthread_mutex_t m_mutex;
};



// 条件变量
class cond
{
public:
  // 初始化条件变量
  cond()
  {
    if (pthread_cond_init(&m_cond, NULL) != 0)
    {
      throw std::exception();
    }
  }
  // 销毁释放资源
  ~cond()
  {
    pthread_cond_destroy(&m_cond);
  }
  // 线程阻塞函数，先把调用该函数的线程放入条件变量的请求队列。
  // 如果线程已经对互斥锁上锁，那么会将这把锁打开，这样做是为了避免死锁。
  // 在线程解除阻塞时，函数内部会帮助这个线程再次将锁锁上，继续向下访问临界区。
  bool wait(pthread_mutex_t *m_mutex)
  {
    int ret = 0;
    // 线程解除阻塞返回0 --> return true
    ret = pthread_cond_wait(&m_cond, m_mutex);
    return ret == 0;
  }
  // 将线程阻塞一定的时间长度，时间到达之后，线程就解除阻塞。
  bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
  {
    int ret = 0;
    ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
    return ret == 0;
  }
  // 唤醒阻塞在条件变量上的线程，至少有一个被解除阻塞
  bool signal()
  {
    return pthread_cond_signal(&m_cond) == 0;
  }
  // 唤醒阻塞在条件变量上的线程，被阻塞的线程全部解除阻塞
  bool broadcast()
  {
    return pthread_cond_broadcast(&m_cond) == 0;
  }

private:
  // 条件变量
  pthread_cond_t m_cond;
};

#endif //GAME_C___LOCK_H
