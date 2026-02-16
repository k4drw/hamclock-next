#include "DatabaseManager.h"
#include "Logger.h"

DatabaseManager &DatabaseManager::instance() {
  static DatabaseManager instance;
  return instance;
}

DatabaseManager::~DatabaseManager() {
  if (db_) {
    sqlite3_close(db_);
  }
}

bool DatabaseManager::init(const std::filesystem::path &dbPath) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_)
    return true; // Already initialized

  if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
    LOG_E("DatabaseManager", "Failed to open database {}: {}", dbPath.string(),
          sqlite3_errmsg(db_));
    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    return false;
  }

  // Create tables if they don't exist
  const char *schema = R"(
    CREATE TABLE IF NOT EXISTS dx_spots (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      tx_call TEXT,
      tx_grid TEXT,
      rx_call TEXT,
      rx_grid TEXT,
      mode TEXT,
      freq_khz REAL,
      snr REAL,
      tx_lat REAL,
      tx_lon REAL,
      rx_lat REAL,
      rx_lon REAL,
      spotted_at INTEGER
    );
    CREATE INDEX IF NOT EXISTS idx_dx_spotted_at ON dx_spots(spotted_at);
    CREATE UNIQUE INDEX IF NOT EXISTS idx_dx_unique ON dx_spots(tx_call, rx_call, freq_khz, spotted_at);
  )";

  char *errMsg = nullptr;
  if (sqlite3_exec(db_, schema, nullptr, nullptr, &errMsg) != SQLITE_OK) {
    LOG_E("DatabaseManager", "Schema creation failed: {}", errMsg);
    sqlite3_free(errMsg);
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }

  LOG_I("DatabaseManager", "Database initialized at {}", dbPath.string());
  return true;
}

bool DatabaseManager::exec(const std::string &sql) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_)
    return false;

  char *errMsg = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
    LOG_E("DatabaseManager", "Exec failed: {}\nSQL: {}", errMsg, sql);
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool DatabaseManager::query(const std::string &sql, QueryCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!db_)
    return false;

  auto cb = [](void *arg, int argc, char **argv, char **colNames) -> int {
    auto *callbackPtr = static_cast<QueryCallback *>(arg);
    Row row;
    row.reserve(argc);
    for (int i = 0; i < argc; ++i) {
      row.push_back(argv[i] ? argv[i] : "");
    }
    if (!(*callbackPtr)(row))
      return 1; // Abort
    return 0;
  };

  char *errMsg = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), cb, &callback, &errMsg) != SQLITE_OK) {
    if (errMsg && std::string(errMsg) != "query aborted") {
      LOG_E("DatabaseManager", "Query failed: {}\nSQL: {}", errMsg, sql);
    }
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}
