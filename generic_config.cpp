
#include <cstdint>
#include <map>
#include <string>

#include "generic_config.hpp"

namespace tsp {

Generic_config::Generic_config() {};

int32_t Generic_config::get_int(std::string key) { return int_vars[key]; };
std::string Generic_config::get_string(std::string key) {
  return str_vars[key];
};
bool Generic_config::get_bool(std::string key) { return bool_vars[key]; };
void Generic_config::set_int(std::string key, uint32_t val) {
  int_vars[key] = val;
};
void Generic_config::set_string(std::string key, std::string val) {
  str_vars[key] = val;
};
void Generic_config::set_bool(std::string key, bool val) {
  bool_vars[key] = val;
};

} // namespace tsp