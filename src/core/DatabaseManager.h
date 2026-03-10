#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <vector>

class DatabaseManager {
public:
  static DatabaseManager &instance();

  // Initialize the database at the given path.
  // Returns true on success.
  bool init(const std::filesystem::path &dbPath);

  // Execute a query that doesn't return rows (INSERT, UPDATE, DELETE, CREATE).
  bool exec(const std::string &sql);

  using Row = std::vector<std::string>;
  using QueryCallback = std::function<bool(const Row &row)>;

  // Execute a query and call callback for each row.
  // Callback should return true to continue, false to stop.
  bool query(const std::string &sql, QueryCallback callback);

  // Execute a non-query statement using a prepared statement.
  // The caller provides a binder function that binds parameters to the stmt
  // before execution (use sqlite3_bind_* inside the lambda).
  // Returns true on success.
  bool execPrepared(const std::string &sql,
                    std::function<void(sqlite3_stmt *)> binder);

private:
  DatabaseManager() = default;
  ~DatabaseManager();

  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

  sqlite3 *db_ = nullptr;
  std::mutex mutex_;
};
