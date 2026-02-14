#include "DXClusterData.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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

DXClusterDataStore::DXClusterDataStore() { loadPersisted(); }

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
  data_.spots.clear();

  db.query(sql, [this](const DatabaseManager::Row &row) {
    if (row.size() < 12)
      return true;
    DXClusterSpot s;
    s.txCall = row[0];
    s.txGrid = row[1];
    s.rxCall = row[2];
    s.rxGrid = row[3];
    s.mode = row[4];
    try {
      s.freqKhz = std::stod(row[5]);
      s.snr = std::stod(row[6]);
      s.txLat = std::stod(row[7]);
      s.txLon = std::stod(row[8]);
      s.rxLat = std::stod(row[9]);
      s.rxLon = std::stod(row[10]);
      int64_t ts = std::stoll(row[11]);
      s.spottedAt =
          std::chrono::system_clock::time_point(std::chrono::seconds(ts));
    } catch (...) {
      return true; // convert error, skip
    }
    data_.spots.push_back(s);
    return true;
  });

  LOG_I("DXClusterDataStore", "Loaded {} persisted spots", data_.spots.size());
}

DXClusterData DXClusterDataStore::get() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

void DXClusterDataStore::set(const DXClusterData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
  // TODO: Full replace in DB? Usually we just add spots incrementally.
}

void DXClusterDataStore::addSpot(const DXClusterSpot &spot) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Create a copy to modify (dithering)
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

  // Add to memory
  data_.spots.push_back(s);
  data_.lastUpdate = std::chrono::system_clock::now();

  // Persist to DB
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

  pruneOldSpots();
}

void DXClusterDataStore::setConnected(bool connected,
                                      const std::string &status) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_.connected = connected;
  data_.statusMsg = status;
  data_.lastUpdate = std::chrono::system_clock::now();
}

void DXClusterDataStore::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  data_.spots.clear();
  data_.lastUpdate = std::chrono::system_clock::now();
  DatabaseManager::instance().exec("DELETE FROM dx_spots");
}

void DXClusterDataStore::pruneOldSpots() {
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(60); // Default 60 mins

  // Prune memory
  data_.spots.erase(std::remove_if(data_.spots.begin(), data_.spots.end(),
                                   [&](const DXClusterSpot &s) {
                                     return (now - s.spottedAt) > maxAge;
                                   }),
                    data_.spots.end());

  // Prune DB (occasionally? or every time? Let's do it every time for correct
  // sync)
  int64_t cutoffTs = std::chrono::duration_cast<std::chrono::seconds>(
                         (now - maxAge).time_since_epoch())
                         .count();
  std::string sql =
      "DELETE FROM dx_spots WHERE spotted_at <= " + std::to_string(cutoffTs);
  DatabaseManager::instance().exec(sql);
}

void DXClusterDataStore::selectSpot(const DXClusterSpot &spot) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_.hasSelection = true;
  data_.selectedSpot = spot;
}

void DXClusterDataStore::clearSelection() {
  std::lock_guard<std::mutex> lock(mutex_);
  data_.hasSelection = false;
}
