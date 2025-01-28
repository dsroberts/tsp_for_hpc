#pragma once

#include <sqlite3.h>
#include <string_view>

namespace tsp {
  class Sqlite_statement_manager {
public:
  Sqlite_statement_manager(sqlite3 *conn, std::string_view sql);
  Sqlite_statement_manager(sqlite3 *conn, std::string_view sql, bool dostep);
  ~Sqlite_statement_manager();
  int step(bool must_have_row);
  int step();
  sqlite3_stmt *stmt;

private:
  void init(sqlite3 *conn);
  std::string_view sql_;
  int sqlite_ret_;
};
} // namespace tsp