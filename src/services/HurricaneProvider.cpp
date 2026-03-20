#include "HurricaneProvider.h"
#include "../core/Constants.h"
#include <SDL.h>
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

HurricaneProvider::HurricaneProvider(NetworkManager &net,
                                     std::shared_ptr<HurricaneStore> store)
    : net_(net), store_(std::move(store)) {}

void HurricaneProvider::fetch(bool force) {
  lastFetchMs_ = SDL_GetTicks();
  // NHC CurrentStorms.json — free, no key, updated ~every 3-6 hours.
  static const char *kUrl =
      "https://www.nhc.noaa.gov/CurrentStorms.json";

  net_.fetchAsync(
      kUrl,
      [](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([body]() {
          try {
            auto j = json::parse(body);
            auto *update = new HurricaneData();

            // NHC JSON: {"activeStorms": [...]}
            const json *storms = nullptr;
            if (j.contains("activeStorms") && j["activeStorms"].is_array())
              storms = &j["activeStorms"];

            if (storms) {
              for (const auto &s : *storms) {
                HurricaneStorm storm;
                auto str = [&](const char *k) -> std::string {
                  return (s.contains(k) && s[k].is_string())
                             ? s[k].get<std::string>()
                             : "";
                };
                storm.id = str("id");
                storm.name = str("name");
                storm.basin = str("basin");
                storm.advisory = str("headline");
                storm.movement = str("movement");

                if (s.contains("lat") && s["lat"].is_number())
                  storm.lat = s["lat"].get<double>();
                if (s.contains("lon") && s["lon"].is_number())
                  storm.lon = s["lon"].get<double>();
                if (s.contains("intensity") && s["intensity"].is_number())
                  storm.maxWindKt = s["intensity"].get<int>();
                if (s.contains("pressure") && s["pressure"].is_number())
                  storm.pressureMb = s["pressure"].get<int>();
                if (s.contains("category") && s["category"].is_number())
                  storm.category = s["category"].get<int>();

                auto ts = str("lastUpdate");
                storm.lastUpdate = ts.size() > 16 ? ts.substr(0, 16) : ts;

                if (!storm.name.empty())
                  update->storms.push_back(std::move(storm));
              }
            }

            update->valid = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_HURRICANE_DATA_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      1800, force);
}
