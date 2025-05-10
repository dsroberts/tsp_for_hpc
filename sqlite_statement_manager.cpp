#include "sqlite_statement_manager.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

#include "functions.hpp"

namespace tsp {

void exit_with_sqlite_err(const char *msg, std::string_view sql, int ret,
                          sqlite3 *conn) {
  std::cerr << msg << std::endl;
  std::cerr << sql << std::endl;
  std::cerr << ret << ": ";
  std::cerr << sqlite3_errmsg(conn) << std::endl;
  std::exit(EXIT_FAILURE);
}

Sqlite_statement_manager::Sqlite_statement_manager(sqlite3 *conn,
                                                   std::string_view sql,
                                                   bool dostep)
    : sql_(sql), sqlite_ret_(SQLITE_OK), conn_(conn) {
  if ((sqlite_ret_ = sqlite3_prepare_v2(conn_, sql_.data(), sql_.length(),
                                        &stmt, nullptr)) != SQLITE_OK) {
    exit_with_sqlite_err("Could not prepare the following sql statement:", sql_,
                         sqlite_ret_, conn_);
  }
  if (dostep) {
    step_put();
  }
}

Sqlite_statement_manager::Sqlite_statement_manager(sqlite3 *conn,
                                                   std::string_view sql)
    : Sqlite_statement_manager(conn, sql, false) {}

Sqlite_statement_manager::~Sqlite_statement_manager() {
  if ((sqlite_ret_ = sqlite3_finalize(stmt)) != SQLITE_OK) {
    exit_with_sqlite_err("Unable finalize statement:\n", sql_, sqlite_ret_,
                         conn_);
  };
}

/*
Output param specialisations
*/

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx, int32_t &val) {
  val = sqlite3_column_int(stmt, param_idx);
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx, uint32_t &val) {
  val = static_cast<uint32_t>(sqlite3_column_int(stmt, param_idx));
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx, int64_t &val) {
  val = sqlite3_column_int64(stmt, param_idx);
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx, uint64_t &val) {
  val = static_cast<uint64_t>(sqlite3_column_int64(stmt, param_idx));
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::optional<int32_t> &val) {
  val.reset();
  auto tmp = sqlite3_column_text(stmt, param_idx);
  if (!!tmp) {
    val.emplace(sqlite3_column_int(stmt, param_idx));
  }
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::optional<uint32_t> &val) {
  val.reset();
  auto tmp = sqlite3_column_text(stmt, param_idx);
  if (!!tmp) {
    val.emplace(sqlite3_column_int(stmt, param_idx));
  }
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::optional<int64_t> &val) {
  val.reset();
  auto tmp = sqlite3_column_text(stmt, param_idx);
  if (!!tmp) {
    val.emplace(sqlite3_column_int64(stmt, param_idx));
  }
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::optional<uint64_t> &val) {
  val.reset();
  auto tmp = sqlite3_column_text(stmt, param_idx);
  if (!!tmp) {
    val.emplace(sqlite3_column_int64(stmt, param_idx));
  }
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx, std::string &val) {
  val = reinterpret_cast<const char *>(sqlite3_column_text(stmt, param_idx));
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::optional<std::string> &val) {
  val.reset();
  auto tmp = sqlite3_column_text(stmt, param_idx);
  if (tmp[0] != '\0') {
    val.emplace(reinterpret_cast<const char *>(tmp));
  }
}

template <>
void Sqlite_statement_manager::bind_out_param(int param_idx,
                                              std::filesystem::path &val) {
  val = reinterpret_cast<const char *>(sqlite3_column_text(stmt, param_idx));
}

template <>
void Sqlite_statement_manager::bind_out_param(
    int param_idx, std::pair<char **, std::string> &val) {
  val.second.assign(
      reinterpret_cast<const char *>(sqlite3_column_blob(stmt, param_idx)),
      static_cast<size_t>(sqlite3_column_bytes(stmt, param_idx)));
  auto ntokens = 0ul;
  for (const auto &c : val.second) {
    if (c == '\0') {
      ntokens++;
    }
  }
  if (nullptr ==
      (val.first = static_cast<char **>(malloc((ntokens) * sizeof(char *))))) {
    die_with_err_errno("Malloc failed", -1);
  }
  auto ctr = 0ul;
  auto start = 0ul;
  auto end = val.second.find('\0');
  while (ctr < ntokens - 1) {
    val.first[ctr] = &val.second[start];
    start = end + 1;
    end = val.second.find('\0', start);
    ctr++;
  }
  val.first[ntokens - 1] = nullptr;
}
/*
Input param specialisations
*/
template <>
void Sqlite_statement_manager::bind_in_param(int param_idx,
                                             const std::string &val) {
  if ((sqlite_ret_ = sqlite3_bind_text(stmt, param_idx, val.c_str(), -1,
                                       SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind string in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx, std::string &val) {
  if ((sqlite_ret_ = sqlite3_bind_text(stmt, param_idx, val.c_str(), -1,
                                       SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind string in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx,
                                             std::filesystem::path &val) {
  if ((sqlite_ret_ = sqlite3_bind_text(stmt, param_idx, val.c_str(), -1,
                                       SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable path in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx, int32_t &val) {
  if ((sqlite_ret_ = sqlite3_bind_int(stmt, param_idx, val)) != SQLITE_OK) {
    die_with_err("Unable bind int in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx, int64_t &val) {
  if ((sqlite_ret_ = sqlite3_bind_int64(stmt, param_idx, val)) != SQLITE_OK) {
    die_with_err("Unable bind int in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx,
                                             const uint32_t &val) {
  if ((sqlite_ret_ = sqlite3_bind_int(stmt, param_idx, val)) != SQLITE_OK) {
    die_with_err("Unable bind int in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx,
                                             const uint64_t &val) {
  if ((sqlite_ret_ = sqlite3_bind_int64(stmt, param_idx, val)) != SQLITE_OK) {
    die_with_err("Unable bind int in statement", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx,
                                             std::vector<std::string> &val) {
  auto tmp = std::string{};
  for (const auto &i : val) {
    tmp += i;
    tmp += '\0';
  }
  tmp += '\0';
  if ((sqlite_ret_ = sqlite3_bind_blob(stmt, param_idx, tmp.data(), tmp.size(),
                                       SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind raw command", sqlite_ret_);
  }
}

template <>
void Sqlite_statement_manager::bind_in_param(int param_idx, char **&val) {
  std::string tmp;
  for (auto i = 0l; val[i] != nullptr; ++i) {
    tmp += val[i];
    tmp += '\0';
  }
  tmp += '\0';
  if ((sqlite_ret_ = sqlite3_bind_blob(stmt, param_idx, tmp.data(), tmp.size(),
                                       SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind env", sqlite_ret_);
  }
}

} // namespace tsp