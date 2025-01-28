#include "sqlite_statement_manager.hpp"
#include "functions.hpp"

#include <iostream>
#include <string>

namespace tsp {

void exit_with_sqlite_err(const char *msg, std::string_view sql, int ret) {
  std::cerr << msg;
  std::cerr << sql;
  std::cerr << sqlite3_errstr(ret);
  std::exit(EXIT_FAILURE);
}

Sqlite_statement_manager::Sqlite_statement_manager(sqlite3 *conn,
                                                   std::string_view sql,
                                                   bool dostep)
    : sql_(sql), sqlite_ret_(SQLITE_OK) {
  if ((sqlite_ret_ = sqlite3_prepare_v2(conn, sql_.data(), -1, &stmt,
                                        nullptr)) != SQLITE_OK) {
    exit_with_sqlite_err("Could not prepare the following sql statement:\n",
                         sql_, sqlite_ret_);
  }
  if (dostep) {
    step();
  }
}

Sqlite_statement_manager::Sqlite_statement_manager(sqlite3 *conn,
                                                   std::string_view sql)
    : Sqlite_statement_manager(conn, sql, false) {}

Sqlite_statement_manager::~Sqlite_statement_manager() {
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    exit_with_sqlite_err("Unable finalize statement:\n", sql_, sqlite_ret_);
  };
}
int Sqlite_statement_manager::step(bool must_have_row) {
  sqlite_ret_ = sqlite3_step(stmt);
  if (sqlite_ret_ == SQLITE_DONE) {
    if (must_have_row) {
      exit_with_sqlite_err(
          "Statement was expected to return results but did not:\n", sql_,
          sqlite_ret_);
    }
    return sqlite_ret_;
  } else if (sqlite_ret_ == SQLITE_ROW) {
    return sqlite_ret_;
  } else {
    exit_with_sqlite_err("SQLite step for statement failed:\n", sql_,
                         sqlite_ret_);
    return sqlite_ret_;
  }
}
int Sqlite_statement_manager::step() { return step(false); }

} // namespace tsp