#include "CallbookProvider.h"
#include "../core/StringUtils.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string normalizeDate(const std::string &dateStr) {
  // Try MM/DD/YYYY -> YYYY-MM-DD
  int m, d, y;
  if (std::sscanf(dateStr.c_str(), "%d/%d/%d", &m, &d, &y) == 3) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
  }
  return dateStr;
}

CallbookProvider::CallbookProvider(NetworkManager &net,
                                   std::shared_ptr<CallbookStore> store)
    : net_(net), store_(store) {}

void CallbookProvider::lookup(const std::string &callsign) {
  if (callsign.empty())
    return;

  auto result = std::make_shared<CallbookData>();
  result->callsign = callsign;
  result->source = "Aggregating...";

  // Chain the lookups
  fetchCallook(callsign, result, [this, result, callsign]() {
    // If Callook failed or was incomplete, try HamDB
    fetchHamDB(callsign, result, [this, result]() {
      result->valid = true;
      store_->set(*result);
    });
  });
}

void CallbookProvider::fetchCallook(const std::string &callsign,
                                    std::shared_ptr<CallbookData> result,
                                    std::function<void()> onDone) {
  std::string url = "https://callook.info/" + callsign + "/json";

  net_.fetchAsync(url, [onDone, result](std::string body) {
    try {
      if (!body.empty()) {
        auto j = json::parse(body);
        if (j["status"] == "VALID") {
          result->name = j["name"].get<std::string>();
          result->address = j["address"]["line1"].get<std::string>();
          result->city = j["address"]["line2"].get<std::string>();
          result->grid = j["location"]["gridsquare"].get<std::string>();
          result->lat = StringUtils::safe_stof(
              j["location"]["latitude"].get<std::string>());
          result->lon = StringUtils::safe_stof(
              j["location"]["longitude"].get<std::string>());

          if (j.contains("otherInfo")) {
            const auto &oi = j["otherInfo"];
            if (oi.contains("expiryDate"))
              result->expiryDate =
                  normalizeDate(oi["expiryDate"].get<std::string>());
            if (oi.contains("frn"))
              result->frn = oi["frn"].get<std::string>();
          }

          result->source = "Callook.info";
        }
      }
    } catch (...) {
    }
    onDone();
  });
}

void CallbookProvider::fetchHamDB(const std::string &callsign,
                                  std::shared_ptr<CallbookData> result,
                                  std::function<void()> onDone) {
  // HamDB is good for international calls and extra meta
  std::string url = "http://api.hamdb.org/" + callsign + "/json/hamclock-next";

  net_.fetchAsync(url, [onDone, result](std::string body) {
    try {
      if (!body.empty()) {
        auto j = json::parse(body);
        if (j.contains("hamdb") && j["hamdb"]["messages"]["status"] == "OK") {
          auto call = j["hamdb"]["callsign"];

          // Only overwrite if Callook didn't find it or if we want more data
          if (result->name.empty())
            result->name = call["name"].get<std::string>();

          // Fetch social/QSL hints if present (some APIs provide this)
          if (call.contains("lotw"))
            result->lotw = (call["lotw"].get<std::string>() == "Y");

          if (result->source.empty()) {
            result->source = "HamDB.org";
          } else if (result->source != "HamDB.org") {
            result->source += " + HamDB";
          }
        }
      }
    } catch (...) {
    }
    onDone();
  });
}
