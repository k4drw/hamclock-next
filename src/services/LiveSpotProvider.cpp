#include "LiveSpotProvider.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace {

// Parse PSK Reporter XML response, aggregating spot counts per band
// and collecting individual spot records for map plotting.
// If plotReceivers is true (DE mode), we map who heard us (ReceiverLocator,
// ReceiverCallsign). If plotReceivers is false (DX mode), we map who we heard
// (SenderLocator, SenderCallsign).
void parsePSKReporter(const std::string &body, LiveSpotData &data,
                      bool plotReceivers) {
  std::string::size_type pos = 0;
  int total = 0;

  while (pos < body.size()) {
    auto tagStart = body.find("<receptionReport ", pos);
    if (tagStart == std::string::npos)
      break;
    auto tagEnd = body.find("/>", tagStart);
    if (tagEnd == std::string::npos)
      tagEnd = body.find(">", tagStart);
    if (tagEnd == std::string::npos)
      break;

    std::string tag = body.substr(tagStart, tagEnd - tagStart);

    std::string freqStr = StringUtils::extractAttr(tag, "frequency");
    if (!freqStr.empty()) {
      long long freqHz = std::atoll(freqStr.c_str());
      double freqKhz = static_cast<double>(freqHz) / 1000.0;
      int idx = freqToBandIndex(freqKhz);
      if (idx >= 0) {
        data.bandCounts[idx]++;
        total++;

        std::string grid;
        std::string call;

        if (plotReceivers) {
          // We are the sender. Map the receiver.
          grid = StringUtils::extractAttr(tag, "receiverLocator");
          call = StringUtils::extractAttr(tag, "receiverCallsign");
        } else {
          // We are the receiver. Map the sender.
          grid = StringUtils::extractAttr(tag, "senderLocator");
          call = StringUtils::extractAttr(tag, "senderCallsign");
        }

        if (grid.size() >= 4) {
          // Store in generic fields (SpotRecord uses receiverGrid for location)
          data.spots.push_back({freqKhz, grid, call});
          if (data.spots.size() >= 500) {
            LOG_W("LiveSpot", "Too many spots in response, capped at 500");
            break;
          }
        }
      }
    }

    pos = tagEnd + 1;
  }

  LOG_I("LiveSpot", "Parsed {} spots ({} with grids)", total,
        data.spots.size());
}

} // namespace

LiveSpotProvider::LiveSpotProvider(NetworkManager &net,
                                   std::shared_ptr<LiveSpotDataStore> store,
                                   const AppConfig &config,
                                   HamClockState *state,
                                   std::shared_ptr<DXClusterDataStore> dxStore)
    : net_(net), store_(std::move(store)), dxStore_(std::move(dxStore)),
      config_(config), state_(state) {}

void LiveSpotProvider::fetch() {
  switch (config_.liveSpotSource) {
  case LiveSpotSource::WSPR:
    fetchWSPR();
    break;
  case LiveSpotSource::RBN:
    fetchRBN();
    break;
  default:
    fetchPSK();
    break;
  }
}

void LiveSpotProvider::fetchPSK() {
  std::string target;
  if (config_.liveSpotsUseCall) {
    target = config_.callsign;
  } else {
    if (config_.grid.size() < 4) {
      LOG_W("LiveSpot", "Grid too short for PSK query: {}", config_.grid);
      return;
    }
    target = config_.grid.substr(0, 4);
  }

  if (target.empty()) {
    LOG_W("LiveSpot", "No callsign or grid configured, skipping");
    return;
  }

  // Build PSK Reporter URL: spots from/by our location
  auto now = std::time(nullptr);
  int64_t quantizedNow = (static_cast<int64_t>(now) / 300) * 300;
  int64_t windowStart = quantizedNow - (config_.liveSpotsMaxAge * 60);

  std::string param;
  if (config_.liveSpotsOfDe) {
    // We are sender: Who heard ME?
    param = config_.liveSpotsUseCall ? "senderCallsign=" : "senderLocator=";
  } else {
    // We are receiver: Who did I hear? (or who did people in my grid hear)
    param = config_.liveSpotsUseCall ? "receiverCallsign=" : "receiverLocator=";
  }

  std::string url = fmt::format("https://retrieve.pskreporter.info/"
                                "query?{}{}&flowStartSeconds={}&rronly=1",
                                param, target, windowStart);

  LOG_I("LiveSpot", "Fetching PSK {}", url);
  if (state_) {
    state_->services["LiveSpot"].lastError = "Fetching...";
  }

  auto store = store_;
  auto grid = config_.grid;
  auto state = state_;
  bool ofDe = config_.liveSpotsOfDe;
  int maxAge = config_.liveSpotsMaxAge;

  net_.fetchAsync(
      url,
      [store, grid, state, ofDe, maxAge](std::string body) {
        LiveSpotData data;
        data.grid = grid.substr(0, 4);
        data.windowMinutes = maxAge;

        if (!body.empty()) {
          parsePSKReporter(body, data, ofDe);
          if (state) {
            auto &s = state->services["LiveSpot"];
            s.ok = true;
            s.lastSuccess = std::chrono::system_clock::now();
            s.lastError = "";
          }
        } else {
          LOG_W("LiveSpot", "Empty response from PSK Reporter");
          if (state) {
            auto &s = state->services["LiveSpot"];
            s.ok = false;
            s.lastError = "Empty response";
          }
        }

        data.lastUpdated = std::chrono::system_clock::now();
        data.valid = true;
        store->set(data);
      },
      300); // 5 minute cache age
}

// Parse one field from a ClickHouse FORMAT CSV line.
// Fields may be quoted with double-quotes; handles escaped "" inside quotes.
// Advances pos past the field and trailing comma.
static std::string csvField(const std::string &line, size_t &pos) {
  if (pos >= line.size())
    return {};
  std::string result;
  if (line[pos] == '"') {
    ++pos; // skip opening quote
    while (pos < line.size()) {
      if (line[pos] == '"') {
        ++pos;
        if (pos < line.size() && line[pos] == '"') {
          result += '"'; // escaped ""
          ++pos;
        } else {
          break; // closing quote
        }
      } else {
        result += line[pos++];
      }
    }
  } else {
    while (pos < line.size() && line[pos] != ',') {
      result += line[pos++];
    }
  }
  if (pos < line.size() && line[pos] == ',')
    ++pos; // skip comma
  return result;
}

void LiveSpotProvider::fetchWSPR() {
  // db1.wspr.live — ClickHouse HTTP endpoint, returns FORMAT CSV.
  // Matches original HamClock query structure.
  std::string target;
  std::string grid4 =
      config_.grid.size() >= 4 ? config_.grid.substr(0, 4) : std::string{};

  if (config_.liveSpotsUseCall) {
    target = config_.callsign;
    if (target.empty()) {
      LOG_W("LiveSpot", "No callsign configured for WSPR query");
      return;
    }
  } else {
    target = grid4;
    if (target.empty()) {
      LOG_W("LiveSpot", "No grid configured for WSPR query");
      return;
    }
  }

  // Band IDs used by db1.wspr.live (MHz integer, matching kBands order)
  static const int wsprBandIds[kNumBands] = {1,  3,  5,  7,  10, 14,
                                              18, 21, 24, 28, 50, 144};
  std::string bandList;
  for (int i = 0; i < kNumBands; ++i) {
    if (config_.liveSpotsBands & (1u << i)) {
      if (!bandList.empty())
        bandList += ',';
      bandList += std::to_string(wsprBandIds[i]);
    }
  }
  if (bandList.empty())
    bandList = "1,3,5,7,10,14,18,21,24,28,50,144";

  int seconds = config_.liveSpotsMaxAge * 60;

  // "of DE" → I am transmitter (tx), show who heard me (rx as other role)
  // "by DE" → I am receiver (rx),   show who I heard  (tx as other role)
  const char *myRole = config_.liveSpotsOfDe ? "tx" : "rx";
  const char *otherRole = config_.liveSpotsOfDe ? "rx" : "tx";

  std::string condition;
  if (config_.liveSpotsUseCall) {
    condition = fmt::format("{}_sign = '{}'", myRole, target);
  } else {
    condition = fmt::format("{}_loc LIKE '{}%'", myRole, target);
  }

  // SQL columns: time, myRole_loc, myRole_sign, otherRole_loc, otherRole_sign,
  //              mode, freq_hz, snr
  std::string sql = fmt::format(
      "SELECT toUnixTimestamp(time),{0}_loc,{0}_sign,{1}_loc,{1}_sign,"
      "'WSPR',cast(frequency as UInt64),snr "
      "FROM wspr.rx "
      "WHERE time > now()-{2} AND band IN ({3}) AND ({4}) "
      "ORDER BY time DESC LIMIT 500 FORMAT CSV",
      myRole, otherRole, seconds, bandList, condition);

  // Simple URL encoder for the SQL query
  std::string encoded;
  encoded.reserve(sql.size() * 2);
  for (unsigned char c : sql) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", c);
      encoded += buf;
    }
  }

  std::string url = "http://db1.wspr.live/?query=" + encoded;
  LOG_I("LiveSpot", "Fetching WSPR via db1.wspr.live");

  if (state_)
    state_->services["LiveSpot"].lastError = "Fetching...";

  auto store = store_;
  auto myGrid4 = grid4;
  auto state = state_;
  int maxAge = config_.liveSpotsMaxAge;

  net_.fetchAsync(
      url,
      [store, myGrid4, state, maxAge](std::string body) {
        LiveSpotData data;
        data.grid = myGrid4;
        data.windowMinutes = maxAge;

        if (body.empty()) {
          LOG_W("LiveSpot", "Empty response from db1.wspr.live");
          if (state) {
            state->services["LiveSpot"].ok = false;
            state->services["LiveSpot"].lastError = "Empty response";
          }
          data.lastUpdated = std::chrono::system_clock::now();
          data.valid = true;
          store->set(data);
          return;
        }

        // Parse FORMAT CSV: time, myLoc, mySign, otherLoc, otherSign,
        //                   mode, freq_hz, snr
        std::istringstream ss(body);
        std::string line;
        while (std::getline(ss, line)) {
          if (line.empty())
            continue;
          // Strip trailing \r
          if (!line.empty() && line.back() == '\r')
            line.pop_back();

          size_t pos = 0;
          csvField(line, pos); // col 0: time (unused)
          csvField(line, pos); // col 1: my loc (unused for map)
          csvField(line, pos); // col 2: my sign (unused for map)
          std::string otherLoc = csvField(line, pos);  // col 3
          std::string otherSign = csvField(line, pos); // col 4
          csvField(line, pos);                         // col 5: mode
          std::string freqStr = csvField(line, pos);   // col 6: freq Hz

          long long freqHz = std::atoll(freqStr.c_str());
          double freqKhz = static_cast<double>(freqHz) / 1000.0;
          int idx = freqToBandIndex(freqKhz);
          if (idx >= 0) {
            data.bandCounts[idx]++;
            if (otherLoc.size() >= 4) {
              data.spots.push_back({freqKhz, otherLoc, otherSign});
              if (data.spots.size() >= 500)
                break;
            }
          }
        }

        if (state) {
          state->services["LiveSpot"].ok = true;
          state->services["LiveSpot"].lastSuccess =
              std::chrono::system_clock::now();
          state->services["LiveSpot"].lastError = "";
        }
        LOG_I("LiveSpot", "Parsed {} WSPR spots from db1.wspr.live",
              data.spots.size());

        data.lastUpdated = std::chrono::system_clock::now();
        data.valid = true;
        store->set(data);
      },
      300);
}

void LiveSpotProvider::fetchRBN() {
  // RBN data comes from the shared DXClusterDataStore (fed by RBNProvider).
  // We apply DE/DX mode and callsign/grid filtering here, matching the
  // original HamClock behaviour:
  //   "of DE" (ofDe=true)  → I am the spotted station (txCall/txGrid = me)
  //                           → map the skimmers who heard me (rxGrid)
  //   "by DE" (ofDe=false) → I am the skimmer/receiver   (rxCall/rxGrid = me)
  //                           → map the stations I heard   (txGrid)
  if (!dxStore_) {
    LOG_W("LiveSpot", "RBN source selected but no DX store available");
    return;
  }

  const std::string &myCall = config_.callsign;
  std::string myGrid4 =
      config_.grid.size() >= 4 ? config_.grid.substr(0, 4) : config_.grid;
  bool ofDe = config_.liveSpotsOfDe;
  bool useCall = config_.liveSpotsUseCall;

  auto snapshot = dxStore_->snapshot();
  LiveSpotData data;
  data.grid = myGrid4;
  data.windowMinutes = config_.liveSpotsMaxAge;

  auto cutoff = std::chrono::system_clock::now() -
                std::chrono::minutes(config_.liveSpotsMaxAge);

  for (auto &spot : snapshot->spots) {
    if (spot.spottedAt < cutoff)
      continue;

    // Check whether this spot involves "me" in the right role
    bool match = false;
    if (ofDe) {
      // I am the transmitter being spotted
      if (useCall)
        match = (spot.txCall == myCall);
      else
        match = (!myGrid4.empty() && spot.txGrid.size() >= 4 &&
                 spot.txGrid.substr(0, 4) == myGrid4);
    } else {
      // I am the receiving skimmer
      if (useCall)
        match = (spot.rxCall == myCall);
      else
        match = (!myGrid4.empty() && spot.rxGrid.size() >= 4 &&
                 spot.rxGrid.substr(0, 4) == myGrid4);
    }

    if (!match)
      continue;

    int idx = freqToBandIndex(spot.freqKhz);
    if (idx >= 0) {
      data.bandCounts[idx]++;
      // Plot the OTHER station on the map
      const std::string &plotGrid = ofDe ? spot.rxGrid : spot.txGrid;
      const std::string &plotCall = ofDe ? spot.rxCall : spot.txCall;
      if (plotGrid.size() >= 4) {
        data.spots.push_back({spot.freqKhz, plotGrid, plotCall});
        if (data.spots.size() >= 500)
          break;
      }
    }
  }

  if (state_) {
    state_->services["LiveSpot"].ok = true;
    state_->services["LiveSpot"].lastSuccess =
        std::chrono::system_clock::now();
    state_->services["LiveSpot"].lastError = "";
  }

  LOG_I("LiveSpot", "Aggregated {} RBN spots from DX store (ofDe={}, useCall={})",
        data.spots.size(), ofDe, useCall);
  data.lastUpdated = std::chrono::system_clock::now();
  data.valid = true;
  store_->set(data);
}

nlohmann::json LiveSpotProvider::getDebugData() const {
  nlohmann::json j;
  j["callsign"] = config_.callsign;
  j["grid"] = config_.grid;
  j["ofDe"] = config_.liveSpotsOfDe;
  j["useCall"] = config_.liveSpotsUseCall;
  return j;
}
