//
// Created by lianyu on 2022/11/25.
//

#include "include/log.h"

// log.cpp
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "include/log.h"
#include <pthread.h>
using namespace std;

// 构造函数
Log::Log()
{
  // 日志行数
  m_count = 0;
  // 是否同步标志位
  m_is_async = false;
}

// 析构函数
Log::~Log()
{
  if (m_fp != NULL)
  {
    // 关闭文件
    fclose(m_fp);
  }
}

// 初始化日志
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) // file_name为日志文件路径
{
  // 若设置了阻塞队列的最大长度 --> 异步写入方式
  if (max_queue_size >= 1)
  {
    // 设置为异步写入方式
    this->m_is_async = true;
    // 创建阻塞队列
    this->m_log_queue = new block_queue<string>(max_queue_size);
    // 创建写线程来异步写日志
    pthread_t tid;
    pthread_create(&tid, NULL, flush_log_thread, NULL); // flush_log_thread为其回调函数
  }
  // 是否关闭日志
  this->m_close_log = close_log;
  // 日志缓冲区大小
  this->m_log_buf_size = log_buf_size;
  // 日志缓冲区
  this->m_buf = new char[this->m_log_buf_size];
  // 初始化日志缓冲区
  memset(this->m_buf, '\0', this->m_log_buf_size);
  // 日志最大行数
  this->m_split_lines = split_lines;

  // 获得此刻的时间戳
  time_t t = time(NULL);
  // 获取系统时间
  struct tm *sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  // 从后往前找到第一个"/"的位置
  const char *p = strrchr(file_name, '/');

  char log_full_name[256] = {0};

  // 若输入的文件名没有"/"
  if (p == NULL)
  {
    // 自定义日志名为：年_月_日_文件名
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s",
             my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
  }
  else
  {
    // p指针从"/"的位置向后移动一个位置
    strcpy(this->log_name, p + 1);
    // p - file_name + 1相当于日志文件路径的"./"
    strncpy(this->dir_name, file_name, p - file_name + 1);
    // 日志名为：路径年_月_日_文件名
    snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s",
             dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, this->log_name);
  }
  // 日志要按天分类，记录当前是哪一天
  this->m_today = my_tm.tm_mday;
  // 打开log的文件指针
  this->m_fp = fopen(log_full_name, "a");
  if (this->m_fp == NULL)
  {
    return false;
  }

  return true;
}

// 向日志文件写入具体内容
void Log::write_log(int level, const char *format, ...)
{
  struct timeval now = {0, 0};
  // 获得此刻的时间戳
  gettimeofday(&now, NULL);
  // 秒
  time_t t = now.tv_sec;
  // 获取系统时间
  struct tm *sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  // 日志等级
  char s[16] = {0};
  // 日志分级
  switch (level)
  {
    case 0:
      strcpy(s, "[debug]:");
      break;
    case 1:
      strcpy(s, "[info]:");
      break;
    case 2:
      strcpy(s, "[warn]:");
      break;
    case 3:
      strcpy(s, "[erro]:");
      break;
    default:
      strcpy(s, "[info]:");
      break;
  }

  // 以下实现按天分类，超行分文件功能
  /****** 加锁 ******/
  this->m_mutex.lock();

  // 日志行数+1
  this->m_count++;

  // 日志不是今天 或 写入的日志行数是最大行的倍数 --> 创建新日志文件
  if (this->m_today != my_tm.tm_mday || this->m_count % this->m_split_lines == 0)
  {
    // 新日志名
    char new_log[256] = {0};
    // 刷新缓冲区
    fflush(this->m_fp);
    // 关闭日志文件
    fclose(this->m_fp);

    // 日志名中的时间部分
    char tail[16] = {0};
    // 年_月_日
    snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

    // 如果是时间不是今天
    if (this->m_today != my_tm.tm_mday)
    {
      // 路径年_月_日_文件名
      snprintf(new_log, 255, "%s%s%s", this->dir_name, tail, this->log_name);
      // 时间改成今天
      this->m_today = my_tm.tm_mday;
      // 重置日志行数
      this->m_count = 0;
    }
      // 日志行数超过最大行限制
    else
    {
      // count/max_lines为"页号"
      // 路径年_月_日_文件名.页号
      snprintf(new_log, 255, "%s%s%s.%lld", this->dir_name, tail, this->log_name, this->m_count / this->m_split_lines);
    }
    // 重新设置打开日志的文件指针
    this->m_fp = fopen(new_log, "a");
  }

  /****** 解锁 ******/
  this->m_mutex.unlock();

  // 将传入的format参数赋值给可变参数列表类型valst，便于格式化输出
  va_list valst;
  va_start(valst, format);

  string log_str;

  /****** 加锁 ******/
  m_mutex.lock();
  // 写一条日志
  // 年-月-日 时:分:秒.微秒 [日志等级]:
  int n = snprintf(this->m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                   my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                   my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

  // 例：LOG_INFO("%s", "adjust timer once");
  int m = vsnprintf(this->m_buf + n, this->m_log_buf_size - 1, format, valst);
  this->m_buf[n + m] = '\n';
  this->m_buf[n + m + 1] = '\0';
  log_str = this->m_buf;

  /****** 解锁 ******/
  this->m_mutex.unlock();

  // 异步写入日志
  if (m_is_async && !m_log_queue->full())
  {
    // 将日志信息加入阻塞队列
    this->m_log_queue->push(log_str);
  }
    // 同步写入日志
  else
  {
    /****** 加锁 ******/
    m_mutex.lock();

    fputs(log_str.c_str(), m_fp);

    /****** 解锁 ******/
    m_mutex.unlock();
  }

  va_end(valst);
}

// 例子：使用多个输出函数连续进行多次输出到控制台时，上一个数据还在输出缓冲区中时，输出函数就把下一个数据加入输出缓冲区，结果冲掉了原来的数据，出现输出错误。
void Log::flush(void)
{
  /****** 加锁 ******/
  m_mutex.lock();

  // 强制将缓冲区内的数据写入指定的文件
  fflush(m_fp);

  /****** 解锁 ******/
  m_mutex.unlock();
}
