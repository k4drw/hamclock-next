#include "FlareProvider.h"
#include "../network/NetworkManager.h"
#include "../core/WorkerService.h"
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <time.h>

using json = nlohmann::json;

FlareProvider::FlareProvider(NetworkManager &net,
                             std::shared_ptr<FlareDataStore> store)
    : net_(net), store_(std::move(store)) {}

FlareProvider::~FlareProvider() {}

void FlareProvider::fetch() {
  const auto url =
      "https://services.swpc.noaa.gov/json/goes/primary/xray-flares-7-day.json";
  auto store = store_;

  net_.fetchAsync(url, [this, store](const std::string &body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([this, body, store]() {
      FlareData data = parseFlareJSON(body);
      data.valid = true;
      data.last_fetch = time(nullptr);

      if (store) {
        store->set(data);
      }

      lastFetchMs_ = SDL_GetTicks();
    });
  });
}

FlareData FlareProvider::parseFlareJSON(const std::string &json_str) {
  FlareData result;

  try {
    auto j = json::parse(json_str);

    if (!j.is_object()) {
      return result;
    }

    // NOAA format: { "flares": [ { "begin_time": "2300", "peak_time": "2317",
    // "end_time": "2400", "class": "X2.3", ... }, ... ] }
    if (!j.contains("flares") || !j["flares"].is_array()) {
      return result;
    }

    for (const auto &flare_obj : j["flares"]) {
      if (!flare_obj.contains("class") || !flare_obj.contains("peak_time")) {
        continue;
      }

      FlareEvent ev;

      // Parse class: "X2.3" -> "X"
      std::string class_str =
          flare_obj["class"].is_string() ? flare_obj["class"].get<std::string>()
                                          : "";
      if (!class_str.empty()) {
        ev.class_type = class_str.substr(0, 1);  // Just the letter
      }

      // Parse peak time: "2317" (HHMM format)
      std::string peak_str = flare_obj["peak_time"].is_string()
                                 ? flare_obj["peak_time"].get<std::string>()
                                 : "";
      if (peak_str.length() >= 4) {
        int hh = std::stoi(peak_str.substr(0, 2));
        int mm = std::stoi(peak_str.substr(2, 2));
        ev.peak_time_utc = hh * 60 + mm;
      }

      // Parse begin time
      std::string begin_str = flare_obj.contains("begin_time") &&
                                      flare_obj["begin_time"].is_string()
                                  ? flare_obj["begin_time"].get<std::string>()
                                  : "";
      if (begin_str.length() >= 4) {
        int hh = std::stoi(begin_str.substr(0, 2));
        int mm = std::stoi(begin_str.substr(2, 2));
        ev.begin_time_utc = hh * 60 + mm;
      }

      // Parse end time
      std::string end_str = flare_obj.contains("end_time") &&
                                    flare_obj["end_time"].is_string()
                                ? flare_obj["end_time"].get<std::string>()
                                : "";
      if (end_str.length() >= 4) {
        int hh = std::stoi(end_str.substr(0, 2));
        int mm = std::stoi(end_str.substr(2, 2));
        ev.end_time_utc = hh * 60 + mm;
      }

      result.events.push_back(ev);
    }

  } catch (const std::exception &) {
    // JSON parse error
    return result;
  }

  return result;
}
