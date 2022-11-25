#include <iostream>
#include "include/config.h"
void run() {
  std::string cmd("mkdir -p " + activate_flag_file_path);
  int ret = system(cmd.c_str());
  if (ret) {
    std::cout << "create dir error: " << ret << ", :" << strerror(errno) << std::endl;
  }
}

int main() {

  Config config;
  WebServer server;

  //初始化
  server.init(config.PORT, username, password, database, config.LOGWrite,
              config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num,
              config.close_log, config.actor_model);


  //日志
  server.log_write();

  //数据库
  server.sql_pool();

  //线程池
  server.thread_pool();

  //触发模式
  server.trig_mode();

  //监听
  server.eventListen();

  //运行
  server.eventLoop();

  return 0;
}
