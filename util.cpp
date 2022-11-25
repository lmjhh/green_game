//
// Created by lianyu on 2022/11/25.
//

#include "include/util.h"

std::string time_to_datetime(time_t t) {
  struct tm * local_time = localtime(&t);
  char chTime[64] = {0};//2016-10-26 18:35:24
  sprintf(chTime, "%u-%u-%u %u:%u:%u", local_time->tm_year, local_time->tm_mon,
          local_time->tm_mday , local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
  return std::string(chTime);
}