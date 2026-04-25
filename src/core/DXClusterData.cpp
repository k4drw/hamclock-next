#include "DXClusterData.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include <algorithm>
#include <chrono>
#include <sqlite3.h>

DXClusterDataStore::DXClusterDataStore()
    : data_(std::make_shared<DXClusterData>()) {
  loadPersisted();
}

DXClusterDataStore::~DXClusterDataStore() {}

void DXClusterDataStore::loadPersisted() {
  auto &db = DatabaseManager::instance();
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::minutes(60);
  int64_t cutoffTs = std::chrono::duration_cast<std::chrono::seconds>(
                         cutoff.time_since_epoch())
                         .count();

  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->spots.clear();

  static const char *kSelectSql =
      "SELECT tx_call, tx_grid, rx_call, rx_grid, mode, freq_khz, snr, tx_lat, "
      "tx_lon, rx_lat, rx_lon, spotted_at FROM dx_spots WHERE spotted_at > ? ORDER BY spotted_at ASC";

  db.execPrepared(kSelectSql, [cutoffTs, &newData](sqlite3_stmt *stmt) {
    sqlite3_bind_int64(stmt, 1, cutoffTs);

    auto col_str = [&](int col) -> std::string {
      const char *p = reinterpret_cast<const char *>(sqlite3_column_text(stmt, col));
      return p ? p : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      DXClusterSpot s;
      s.txCall  = col_str(0);
      s.txGrid  = col_str(1);
      s.rxCall  = col_str(2);
      s.rxGrid  = col_str(3);
      s.mode    = col_str(4);
      s.freqKhz = sqlite3_column_double(stmt, 5);
      s.snr     = sqlite3_column_double(stmt, 6);
      s.txLat   = sqlite3_column_double(stmt, 7);
      s.txLon   = sqlite3_column_double(stmt, 8);
      s.rxLat   = sqlite3_column_double(stmt, 9);
      s.rxLon   = sqlite3_column_double(stmt, 10);
      int64_t ts = sqlite3_column_int64(stmt, 11);
      s.spottedAt =
          std::chrono::system_clock::time_point(std::chrono::seconds(ts));
      newData->spots.push_back(s);
    }
  });

  data_ = newData;
  LOG_I("DXClusterDataStore", "Loaded {} persisted spots", data_->spots.size());
}

std::shared_ptr<const DXClusterData> DXClusterDataStore::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

void DXClusterDataStore::addSpot(const DXClusterSpot &spot) {
  // Create a dithered copy of the spot
  DXClusterSpot s = spot;
  // Apply dithering to prevent stacking
  // +/- ~0.5 degree (approx 2 pixels on 800px wide map)
  if (s.txLat != 0 || s.txLon != 0) {
    s.txLat += (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.5f;
    s.txLon += (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.5f;
  }
  if (s.rxLat != 0 || s.rxLon != 0) {
    s.rxLat += (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.5f;
    s.rxLon += (static_cast<float>(rand() % 100) / 50.0f - 1.0f) * 0.5f;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create a single copy to modify
    auto newData = std::make_shared<DXClusterData>(*data_);

    // 1. Add the new spot to the copy
    newData->spots.push_back(s);
    newData->lastUpdate = std::chrono::system_clock::now();

    // 2. Prune old spots from the same copy (in-place)
    auto now = std::chrono::system_clock::now();
    auto maxAge = std::chrono::minutes(maxAgeMinutes_);
    newData->spots.erase(
        std::remove_if(newData->spots.begin(), newData->spots.end(),
                       [&](const DXClusterSpot &spot_to_prune) {
                         return (now - spot_to_prune.spottedAt) > maxAge;
                       }),
        newData->spots.end());

    // 3. Hard count cap — prevent unbounded growth on busy clusters (RPi
    // safety)
    static constexpr size_t kMaxDxSpots = 500;
    if (newData->spots.size() > kMaxDxSpots)
      newData->spots.erase(
          newData->spots.begin(),
          newData->spots.begin() +
              static_cast<ptrdiff_t>(newData->spots.size() - kMaxDxSpots));

    // 3. Atomically swap the main pointer
    data_ = newData;
  }

  // Persist to DB (outside the lock)
  auto &db = DatabaseManager::instance();
  int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(
                   s.spottedAt.time_since_epoch())
                   .count();

  static const char *kInsertSql =
      "INSERT OR IGNORE INTO dx_spots "
      "(tx_call, tx_grid, rx_call, rx_grid, mode, "
      "freq_khz, snr, tx_lat, tx_lon, rx_lat, rx_lon, spotted_at) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)";

  db.execPrepared(kInsertSql, [&s, ts](sqlite3_stmt *stmt) {
    sqlite3_bind_text(stmt, 1, s.txCall.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.txGrid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.rxCall.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, s.rxGrid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, s.mode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, s.freqKhz);
    sqlite3_bind_double(stmt, 7, s.snr);
    sqlite3_bind_double(stmt, 8, s.txLat);
    sqlite3_bind_double(stmt, 9, s.txLon);
    sqlite3_bind_double(stmt, 10, s.rxLat);
    sqlite3_bind_double(stmt, 11, s.rxLon);
    sqlite3_bind_int64(stmt, 12, ts);
  });

  pruneOldSpots(); // This now only prunes the DB
}

void DXClusterDataStore::setConnected(bool connected,
                                      const std::string &status) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->connected = connected;
  newData->statusMsg = status;
  newData->lastUpdate = std::chrono::system_clock::now();
  data_ = newData;
}

void DXClusterDataStore::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->spots.clear();
  newData->lastUpdate = std::chrono::system_clock::now();
  data_ = newData;
  DatabaseManager::instance().exec("DELETE FROM dx_spots");
}

void DXClusterDataStore::setMaxAgeMinutes(int minutes) {
  std::lock_guard<std::mutex> lock(mutex_);
  maxAgeMinutes_ = std::clamp(minutes, 10, 60);
}

void DXClusterDataStore::pruneInMemory() {
  auto now = std::chrono::system_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  auto maxAge = std::chrono::minutes(maxAgeMinutes_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  auto before = newData->spots.size();
  newData->spots.erase(
      std::remove_if(newData->spots.begin(), newData->spots.end(),
                     [&](const DXClusterSpot &s) {
                       return (now - s.spottedAt) > maxAge;
                     }),
      newData->spots.end());
  if (newData->spots.size() != before)
    data_ = newData;
}

void DXClusterDataStore::pruneOldSpots() {
  // Prune DB only. In-memory pruning is now done in addSpot.
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(maxAgeMinutes_);

  int64_t cutoffTs = std::chrono::duration_cast<std::chrono::seconds>(
                         (now - maxAge).time_since_epoch())
                         .count();
  
  static const char *kPruneSql = "DELETE FROM dx_spots WHERE spotted_at <= ?";
  DatabaseManager::instance().execPrepared(kPruneSql, [cutoffTs](sqlite3_stmt *stmt) {
    sqlite3_bind_int64(stmt, 1, cutoffTs);
  });
}

void DXClusterDataStore::setSpots(const std::vector<DXClusterSpot> &spots) {
  std::lock_guard<std::mutex> lk(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->spots = spots;
  newData->lastUpdate = std::chrono::system_clock::now();
  data_ = newData;
}

void DXClusterDataStore::selectSpot(const DXClusterSpot &spot) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->hasSelection = true;
  newData->selectedSpot = spot;
  data_ = newData;
}

void DXClusterDataStore::clearSelection() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->hasSelection = false;
  data_ = newData;
}

void DXClusterDataStore::setWatchedCall(const std::string &call) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->watchedCall = call;
  newData->watchedSpotted = false;
  data_ = newData;
}

void DXClusterDataStore::setWatchedSpotted(std::chrono::steady_clock::time_point when) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->watchedSpotted = true;
  newData->watchedSpottedAt = when;
  data_ = newData;
}

void DXClusterDataStore::clearWatchedSpotted() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->watchedSpotted = false;
  data_ = newData;
}
