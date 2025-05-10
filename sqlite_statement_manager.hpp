#pragma once

#include <sqlite3.h>
#include <string_view>

namespace tsp {

void exit_with_sqlite_err(const char *msg, std::string_view sql, int ret,
                          sqlite3 *conn);
class Sqlite_statement_manager {
public:
  Sqlite_statement_manager(sqlite3 *conn, std::string_view sql);
  Sqlite_statement_manager(sqlite3 *conn, std::string_view sql, bool dostep);
  ~Sqlite_statement_manager();
  sqlite3_stmt *stmt;
  /*
  Output interface
  */
  template <typename... Targs>
  void step_get(bool must_have_row, Targs &...Fargs) {
    if (must_have_row && !step_get(Fargs...)) {
      exit_with_sqlite_err(
          "Statement was expected to return results but did not:\n", sql_,
          sqlite_ret_, conn_);
    }
  }
  template <typename... Targs> bool step_get(Targs &...Fargs) {
    int param_idx{0};
    sqlite_ret_ = sqlite3_step(stmt);
    if (sqlite_ret_ != SQLITE_DONE && sqlite_ret_ != SQLITE_ROW) {
      exit_with_sqlite_err("SQLite step failed:\n", sql_, sqlite_ret_, conn_);
    }
    if (sqlite_ret_ == SQLITE_DONE) {
      return false;
    }
    bind_out_params(param_idx, Fargs...);
    return true;
  }
  /*
  Input interface
  */
  template <typename... Targs> void step_put(Targs &...Fargs) {
    int param_idx{1};
    bind_in_params(param_idx, Fargs...);
  }

private:
  const std::string_view sql_;
  int sqlite_ret_;
  sqlite3 *conn_;
  /*
  Output param binding
  */
  template <typename T> void bind_out_param(int param_idx, T &val);
  void bind_out_params(int param_idx) {};
  template <typename T, typename... Targs>
  void bind_out_params(int param_idx, T &val, Targs &...Fargs) {
    bind_out_param(param_idx, val);
    param_idx++;
    bind_out_params(param_idx, Fargs...);
  }
  /*
  Input param binding
  */
  template <typename T> void bind_in_param(int param_idx, T &val);
  void bind_in_params(int param_idx) {
    sqlite_ret_ = sqlite3_step(stmt);
    if (sqlite_ret_ != SQLITE_DONE) {
      exit_with_sqlite_err("SQLite step for statement failed:\n", sql_,
                           sqlite_ret_, conn_);
    }
    if ((sqlite_ret_ = sqlite3_reset(stmt)) != SQLITE_OK) {
      exit_with_sqlite_err("SQLite rest failed:\n", sql_, sqlite_ret_, conn_);
    };
  }
  template <typename T, typename... Targs>
  void bind_in_params(int param_idx, T &val, Targs &...Fargs) {
    bind_in_param(param_idx, val);
    param_idx++;
    bind_in_params(param_idx, Fargs...);
  }
};
} // namespace tsp