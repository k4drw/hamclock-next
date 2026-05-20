#include "CallbookProvider.h"
#include "../core/StringUtils.h"
#include "../core/TimeUtils.h"
#include <SDL.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string normalizeDate(const std::string &dateStr) {
  // Try MM/DD/YYYY -> YYYY-MM-DD
  int m, d, y;
  if (std::sscanf(dateStr.c_str(), "%d/%d/%d", &m, &d, &y) == 3) {
    return TimeUtils::dateISO(y, m, d);
  }
  return dateStr;
}

// Static helpers avoid capturing `this` in async callbacks.
// CallbookProvider lives in DashboardContext which may be reset while
// network callbacks are in-flight; static helpers + explicit captures
// ensure no dangling pointer access.

static void fetchHamDBAsync(NetworkManager &net, const std::string &callsign,
                             std::shared_ptr<CallbookData> result,
                             std::function<void()> onDone) {
  std::string url =
      "http://api.hamdb.org/" + callsign + "/json/hamclock-next";

  net.fetchAsync(url, [onDone, result](std::string body) {
    try {
      if (!body.empty()) {
        auto j = json::parse(body);
        if (j.contains("hamdb") &&
            j["hamdb"]["messages"]["status"] == "OK") {
          auto call = j["hamdb"]["callsign"];

          if (result->name.empty())
            result->name = call["name"].get<std::string>();

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

static void fetchCallookAsync(NetworkManager &net, const std::string &callsign,
                               std::shared_ptr<CallbookData> result,
                               std::function<void()> onDone) {
  std::string url = "https://callook.info/" + callsign + "/json";

  net.fetchAsync(url, [onDone, result](std::string body) {
    try {
      if (!body.empty()) {
        auto j = json::parse(body);
        if (j["status"] == "VALID") {
          result->name    = j["name"].get<std::string>();
          result->address = j["address"]["line1"].get<std::string>();
          result->city    = j["address"]["line2"].get<std::string>();
          result->grid    = j["location"]["gridsquare"].get<std::string>();
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

CallbookProvider::CallbookProvider(NetworkManager &net,
                                   std::shared_ptr<CallbookStore> store)
    : net_(net), store_(store) {}

void CallbookProvider::lookup(const std::string &callsign) {
  lastFetchMs_ = SDL_GetTicks();
  if (callsign.empty())
    return;

  auto result = std::make_shared<CallbookData>();
  result->callsign = callsign;
  result->source   = "Aggregating...";

  // Capture store_ by value (shared_ptr copy; CallbookStore lives in AppContext
  // and outlasts DashboardContext) and net_ as a raw pointer (NetworkManager is
  // also AppContext-lifetime). Do NOT capture `this`: CallbookProvider lives in
  // DashboardContext and may be destroyed before these callbacks fire.
  auto store = store_;
  auto *net  = &net_;

  fetchCallookAsync(*net, callsign, result,
                    [net, result, callsign, store]() {
                      fetchHamDBAsync(*net, callsign, result,
                                      [store, result]() {
                                        result->valid = true;
                                        store->set(*result);
                                      });
                    });
}
