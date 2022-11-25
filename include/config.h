//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___CONFIG_H
#define GAME_C___CONFIG_H
#include <string>
#include "web_server.h"

const std::string server = "localhost";
const std::string username = "root";
const std::string password = "111111";
const std::string database = "atec2022";
const std::string activate_flag_file_path = "/home/admin/workspace/job/output/user_activated";

using namespace std;
class Config
{
public:
  Config(){
    //端口号,默认9006
    PORT = 8080;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    TRIGMode = 0;

    //listenfd触发模式，默认LT
    LISTENTrigmode = 0;

    //connfd触发模式，默认LT
    CONNTrigmode = 0;

    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是proactor
    actor_model = 0;
  }

  ~Config(){};

  void parse_arg(int argc, char*argv[]);

  //端口号
  int PORT;

  //日志写入方式
  int LOGWrite;

  //触发组合模式
  int TRIGMode;

  //listenfd触发模式
  int LISTENTrigmode;

  //connfd触发模式
  int CONNTrigmode;

  //优雅关闭链接
  int OPT_LINGER;

  //数据库连接池数量
  int sql_num;

  //线程池内的线程数量
  int thread_num;

  //是否关闭日志
  int close_log;

  //并发模型选择
  int actor_model;
};

#endif //GAME_C___CONFIG_H
