//
// Created by lianyu on 2022/11/25.
//

#ifndef GAME_C___TOTAL_TABLE_H
#define GAME_C___TOTAL_TABLE_H
#include <string>
#include <time.h>
class TotalEngrey {
public:
  static std::string get_table_name () {
    return "total_energy";
  }

  int get_id() {
    return id_;
  }

  void set_id(int id) {
    id_ = id;
  }

  time_t get_create_time() {
    return gmt_create_;
  }

  void set_create_time(time_t create_time) {
    gmt_create_ = create_time;
  }

  time_t get_modifi_time() {
    return gmt_modified_;
  }

  void set_modifi_time(time_t modifi_time) {
    gmt_modified_ = modifi_time;
  }

  std::string get_user_id() {
    return user_id_;
  }

  void set_user_id(std::string user_id) {
    user_id_ = user_id;
  }

  int get_total_energy() {
    return total_energy_;
  }

  void set_total_energy(int total_energy) {
    total_energy_ = total_energy;
  }

private:
  int id_;
  time_t gmt_create_;
  time_t gmt_modified_;
  std::string user_id_;
  int total_energy_;
};

#endif //GAME_C___TOTAL_TABLE_H
