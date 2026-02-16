#include "LiveSpotProvider.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"

#include <chrono>
#include <cstdlib>
#include <ctime>

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
                                   HamClockState *state)
    : net_(net), store_(std::move(store)), config_(config), state_(state) {}

void LiveSpotProvider::fetch() {
  std::string target;
  if (config_.pskUseCall) {
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
  int64_t windowStart = quantizedNow - (config_.pskMaxAge * 60);

  std::string param;
  if (config_.pskOfDe) {
    // We are sender: Who heard ME?
    param = config_.pskUseCall ? "senderCallsign=" : "senderLocator=";
  } else {
    // We are receiver: Who did I hear? (or who did people in my grid hear)
    param = config_.pskUseCall ? "receiverCallsign=" : "receiverLocator=";
  }

  std::string url = fmt::format("https://retrieve.pskreporter.info/"
                                "query?{}{}&flowStartSeconds={}&rronly=1",
                                param, target, windowStart);

  LOG_I("LiveSpot", "Fetching {}", url);
  if (state_) {
    state_->services["LiveSpot"].lastError = "Fetching...";
  }

  auto store = store_;
  auto grid = config_.grid;
  auto state = state_;
  bool ofDe = config_.pskOfDe;

  net_.fetchAsync(
      url,
      [store, grid, state, ofDe](std::string body) {
        LiveSpotData data;
        data.grid = grid.substr(0, 4);
        data.windowMinutes = 30; // TODO: from config

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

nlohmann::json LiveSpotProvider::getDebugData() const {
  nlohmann::json j;
  j["callsign"] = config_.callsign;
  j["grid"] = config_.grid;
  j["ofDe"] = config_.pskOfDe;
  j["useCall"] = config_.pskUseCall;
  return j;
}
