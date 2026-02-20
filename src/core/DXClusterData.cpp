#include "DXClusterData.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace {
std::string sqlEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\'')
      out += "''";
    else
      out += c;
  }
  return out;
}
} // namespace

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

  std::string sql =
      "SELECT tx_call, tx_grid, rx_call, rx_grid, mode, freq_khz, snr, tx_lat, "
      "tx_lon, rx_lat, rx_lon, spotted_at FROM dx_spots WHERE spotted_at > " +
      std::to_string(cutoffTs);

  std::lock_guard<std::mutex> lock(mutex_);
  auto newData = std::make_shared<DXClusterData>(*data_);
  newData->spots.clear();

  db.query(sql, [this, &newData](const DatabaseManager::Row &row) {
    if (row.size() < 12)
      return true;
    DXClusterSpot s;
    s.txCall = row[0];
    s.txGrid = row[1];
    s.rxCall = row[2];
    s.rxGrid = row[3];
    s.mode = row[4];
    s.freqKhz = StringUtils::safe_stod(row[5]);
    s.snr = StringUtils::safe_stod(row[6]);
    s.txLat = StringUtils::safe_stod(row[7]);
    s.txLon = StringUtils::safe_stod(row[8]);
    s.rxLat = StringUtils::safe_stod(row[9]);
    s.rxLon = StringUtils::safe_stod(row[10]);
    int64_t ts = std::stoll(row[11]);
    s.spottedAt =
        std::chrono::system_clock::time_point(std::chrono::seconds(ts));
    newData->spots.push_back(s);
    return true;
  });

  data_ = newData;
  LOG_I("DXClusterDataStore", "Loaded {} persisted spots", data_->spots.size());
}

std::shared_ptr<const DXClusterData> DXClusterDataStore::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

void DXClusterDataStore::set(const DXClusterData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = std::make_shared<DXClusterData>(data);
  // TODO: Full replace in DB? Usually we just add spots incrementally.
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
    auto maxAge = std::chrono::minutes(60);
    newData->spots.erase(std::remove_if(newData->spots.begin(),
                                        newData->spots.end(),
                                        [&](const DXClusterSpot &spot_to_prune) {
                                          return (now - spot_to_prune.spottedAt) > maxAge;
                                        }),
                         newData->spots.end());
    
    // 3. Atomically swap the main pointer
    data_ = newData;
  }

  // Persist to DB (outside the lock)
  auto &db = DatabaseManager::instance();
  int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(
                   s.spottedAt.time_since_epoch())
                   .count();

  std::stringstream ss;
  ss << "INSERT OR IGNORE INTO dx_spots (tx_call, tx_grid, rx_call, rx_grid, "
        "mode, "
        "freq_khz, snr, tx_lat, tx_lon, rx_lat, rx_lon, spotted_at) VALUES ('"
     << sqlEscape(s.txCall) << "', '" << sqlEscape(s.txGrid) << "', '"
     << sqlEscape(s.rxCall) << "', '" << sqlEscape(s.rxGrid) << "', '"
     << sqlEscape(s.mode) << "', " << s.freqKhz << ", " << s.snr << ", "
     << s.txLat << ", " << s.txLon << ", " << s.rxLat << ", " << s.rxLon << ", "
     << ts << ")";

  db.exec(ss.str());

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

void DXClusterDataStore::pruneOldSpots() {
  // Prune DB only. In-memory pruning is now done in addSpot.
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(60);

  int64_t cutoffTs = std::chrono::duration_cast<std::chrono::seconds>(
                         (now - maxAge).time_since_epoch())
                         .count();
  std::string sql =
      "DELETE FROM dx_spots WHERE spotted_at <= " + std::to_string(cutoffTs);
  DatabaseManager::instance().exec(sql);
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
