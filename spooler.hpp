#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace tsp {

class Spooler_config {
public:
  Spooler_config();
  int32_t get_int(std::string key);
  std::string get_string(std::string key);
  bool get_bool(std::string key);
  void set_int(std::string key, uint32_t val);
  void set_string(std::string key, std::string val);
  void set_bool(std::string key, bool val);

private:
  std::map<std::string, bool> bool_vars{};
  std::map<std::string, std::int32_t> int_vars{};
  std::map<std::string, std::string> str_vars{};
};
int do_spooler(Spooler_config conf, int argc, int optind, char *argv[]);
} // namespace tsp