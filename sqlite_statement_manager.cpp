#include "sqlite_statement_manager.hpp"

#include <iostream>
#include <sqlite3.h>
#include <string_view>

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
  if ((sqlite_ret_ = sqlite3_prepare_v2(conn_, sql_.data(), -1, &stmt,
                                        nullptr)) != SQLITE_OK) {
    exit_with_sqlite_err("Could not prepare the following sql statement:", sql_,
                         sqlite_ret_, conn_);
  }
  if (dostep) {
    step(false);
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
int Sqlite_statement_manager::step(bool must_have_row) {
  sqlite_ret_ = sqlite3_step(stmt);
  if (sqlite_ret_ == SQLITE_DONE) {
    if (must_have_row) {
      exit_with_sqlite_err(
          "Statement was expected to return results but did not:\n", sql_,
          sqlite_ret_, conn_);
    }
  } else if (sqlite_ret_ != SQLITE_ROW) {
    exit_with_sqlite_err("SQLite step for statement failed:\n", sql_,
                         sqlite_ret_, conn_);
  }
  return sqlite_ret_;
}
int Sqlite_statement_manager::step() { return step(false); }
int Sqlite_statement_manager::step_and_reset() {
  step();
  if ((sqlite_ret_ = sqlite3_reset(stmt)) != SQLITE_OK) {
    exit_with_sqlite_err("SQLite rest failed:\n", sql_, sqlite_ret_, conn_);
  };
  return sqlite_ret_;
}

} // namespace tsp