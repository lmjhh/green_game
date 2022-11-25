//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___LOG_H
#define GAME_C___LOG_H

// log.h

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log
{
public:
  // 懒汉式单例模式
  // C++11编译器保证函数内的局部静态对象的线程安全性
  static Log *get_instance()
  {
    static Log instance;
    return &instance;
  }
  // 异步写入方式公有方法
  static void *flush_log_thread(void *args)
  {
    Log::get_instance()->async_write_log();
  }

  // 初始化日志
  bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
  // 向日志文件写入具体内容
  void write_log(int level, const char *format, ...);
  // 强制刷新缓冲区
  void flush(void);

private:
  // 构造函数
  Log();
  // 析构函数
  virtual ~Log();
  // 异步写入日志方法
  void *async_write_log()
  {
    string single_log;
    // 从阻塞队列中取出一条日志
    while (m_log_queue->pop(single_log))
    {
      /****** 加锁 ******/
      m_mutex.lock();

      // 写进日志文件
      fputs(single_log.c_str(), m_fp);

      /****** 解锁 ******/
      m_mutex.unlock();
    }
  }

private:
  // 日志文件所在最内层文件夹
  char dir_name[128];
  // 日志文件名
  char log_name[128];
  // 日志最大行数
  int m_split_lines;
  // 日志缓冲区大小
  int m_log_buf_size;
  // 日志行数
  long long m_count;
  // 由于日志按天分类，记录当前是哪一天
  int m_today;
  // 打开日志的文件指针
  FILE *m_fp;
  // 日志缓冲区
  char *m_buf;
  // 阻塞队列
  block_queue<string> *m_log_queue;
  // 是否同步标志位
  bool m_is_async;
  // 互斥锁
  locker m_mutex;
  // 是否关闭日志
  int m_close_log;
};

// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
// __VA_ARGS__是一个可变参数的宏
// __VA_ARGS__宏前面加上##的作用在于，当可变参数的个数为0时，会把前面多余的","去掉，否则会编译出错。

// DEBUG
#define LOG_DEBUG(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

// INFO
#define LOG_INFO(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

// WARN
#define LOG_WARN(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

// ERROR
#define LOG_ERROR(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }


#endif //GAME_C___LOG_H
