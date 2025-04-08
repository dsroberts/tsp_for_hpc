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
  int step(bool must_have_row);
  int step();
  int step_and_reset();
  sqlite3_stmt *stmt;

private:
  std::string_view sql_;
  int sqlite_ret_;
  sqlite3 *conn_;
};
} // namespace tsp