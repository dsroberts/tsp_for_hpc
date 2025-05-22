#pragma once

#include <optional>
#include <sqlite3.h>
#include <string_view>
#include <tuple>

#include <iostream>

namespace tsp {

constexpr int sql_param_out = 0;
constexpr int sql_param_in = 1;

void exit_with_sqlite_err(std::string_view msg, int ret, std::string_view stmt,
                          sqlite3 *conn);
void exit_with_sqlite_err(std::string_view msg, int ret, sqlite3_stmt *stmt,
                          sqlite3 *conn);
class Sqlite_statement_manager {
public:
  Sqlite_statement_manager(sqlite3 *conn, std::string_view sql);
  ~Sqlite_statement_manager();
  /*
  I/O interface
  */
  template <typename... Oargs, typename... Iargs>
  auto step(Iargs &&...InParams) {
    if (sqlite_ret_ != SQLITE_ROW) {
      if constexpr (sizeof...(InParams) > 0) {
        auto tup = std::make_tuple(InParams...);
        bind_params<sql_param_in, 0, sizeof...(InParams)>(tup);
      }
    }
    sqlite_ret_ = sqlite3_step(stmt_);
    if (sqlite_ret_ != SQLITE_DONE && sqlite_ret_ != SQLITE_ROW) {
      exit_with_sqlite_err("SQLite step failed:", sqlite_ret_, stmt_, conn_);
    }
    if constexpr (sizeof...(Oargs) == 0) {
      if (sqlite_ret_ == SQLITE_DONE) {
        if ((sqlite_ret_ = sqlite3_reset(stmt_)) != SQLITE_OK) {
          exit_with_sqlite_err("SQLite reset failed:", sqlite_ret_, stmt_,
                               conn_);
        }
      }
      return;
    } else if constexpr (sizeof...(Oargs) == 1) {
      std::optional<Oargs...> out;
      if (sqlite_ret_ == SQLITE_ROW) {
        std::tuple<Oargs...> tmp;
        bind_params<sql_param_out, 0, 1>(tmp);
        out = std::get<0>(tmp);
      }
      if (sqlite_ret_ == SQLITE_DONE) {
        if ((sqlite_ret_ = sqlite3_reset(stmt_)) != SQLITE_OK) {
          exit_with_sqlite_err("SQLite reset failed:", sqlite_ret_, stmt_,
                               conn_);
        }
      }
      return out;
    } else {
      std::optional<std::tuple<Oargs...>> out;
      if (sqlite_ret_ == SQLITE_ROW) {
        std::tuple<Oargs...> tmp;
        bind_params<sql_param_out, 0, sizeof...(Oargs)>(tmp);
        out = tmp;
      }
      if (sqlite_ret_ == SQLITE_DONE) {
        if ((sqlite_ret_ = sqlite3_reset(stmt_)) != SQLITE_OK) {
          exit_with_sqlite_err("SQLite reset failed:", sqlite_ret_, stmt_,
                               conn_);
        }
      }
      return out;
    }
  }

  template <typename... Oargs, typename... Iargs>
  auto fetch_one(Iargs &&...InParams) {
    static_assert(sizeof...(Oargs) > 0,
                  "Cannot use fetch_one with no output parameters provided");
    auto tmp = step<Oargs...>(InParams...);
    if (!tmp) {
      exit_with_sqlite_err("No result matching this statement was found",
                           sqlite_ret_, stmt_, conn_);
    }
    return tmp.value();
  }

private:
  int sqlite_ret_;
  sqlite3_stmt *stmt_;
  sqlite3 *conn_;
  template <int I, typename T> void bind_param(int param_idx, T &val);
  template <int I, size_t J, size_t Size, typename... Targs>
  void bind_params(std::tuple<Targs...> &args) {
    // sqlite input parameter indices start at 1
    bind_param<I>(J + I, std::get<J>(args));
    if constexpr (J < Size - 1) {
      bind_params<I, J + 1, Size>(args);
    }
  }
};
} // namespace tsp