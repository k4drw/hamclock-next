// ActivityProvider — Intentional architectural exception: uses SDL_PushEvent to
// deliver burst/async activity data (DX Peditions, POTA, SOTA) rather than
// Store-Push. The event-driven model is appropriate here because updates are
// infrequent and the payload must be processed on the SDL main thread anyway.
//
// fetchDXPeds() parses HTML from ng3k.com (Announced DX Operations). No JSON
// API is available from this source; HTML scraping is the only option. Any
// upstream HTML layout change will silently break DX Peditions data ingestion.
#include "ActivityProvider.h"
#include "../core/ActivityLocationManager.h"
#include "../core/Astronomy.h"
#include "../core/Constants.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ActivityProvider::ActivityProvider(NetworkManager &net,
                                   std::shared_ptr<ActivityDataStore> store)
    : net_(net), store_(store) {}

void ActivityProvider::fetch() {
  fetchDXPeds();
  fetchPOTA();
  fetchSOTA();
}

void ActivityProvider::fetchDXPeds() {
  net_.fetchAsync(DX_PEDS_URL, [](std::string data) {
    if (data.empty()) {
      LOG_E("ActivityProvider", "Failed to fetch DXPeditions from NG3K");
      return;
    }

    WorkerService::getInstance().submitTask([data]() {
      auto *update = new ActivityData();
      auto now = std::chrono::system_clock::now();

      auto crackMonth = [](const std::string &m) -> int {
        static const char *months[] = {"Jan", "Feb", "Mar", "Apr",
                                       "May", "Jun", "Jul", "Aug",
                                       "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
          if (m.find(months[i]) != std::string::npos)
            return i + 1;
        }
        return 0;
      };

      size_t pos = 0;
      while ((pos = data.find("class=\"adxoitem\"", pos)) !=
             std::string::npos) {
        auto findTagContent = [&](const std::string &html,
                                  const std::string &className,
                                  size_t &searchPos) -> std::string {
          std::string target = "class=\"" + className + "\"";
          size_t p = html.find(target, searchPos);
          if (p == std::string::npos)
            return "";
          size_t start = html.find(">", p);
          if (start == std::string::npos)
            return "";
          start++;
          size_t end = html.find("<", start);
          if (end == std::string::npos)
            return "";
          // Dig through nested empty-content tags (e.g. <span><a>text</a></span>)
          for (int depth = 0; depth < 3 && start == end; ++depth) {
            size_t nxt = html.find(">", end);
            if (nxt == std::string::npos) return "";
            start = nxt + 1;
            end = html.find("<", start);
            if (end == std::string::npos) return "";
          }
          searchPos = end;
          return html.substr(start, end - start);
        };

        size_t rowPos = pos;
        std::string d1 = findTagContent(data, "date", rowPos);
        std::string d2 = findTagContent(data, "date", rowPos);
        std::string loc = findTagContent(data, "cty", rowPos);
        std::string call = findTagContent(data, "call", rowPos);

        if (call.find("<a") != std::string::npos) {
          size_t a_end = call.find(">");
          if (a_end != std::string::npos) {
            size_t a_close = call.find("</a", a_end);
            if (a_close != std::string::npos) {
              call = call.substr(a_end + 1, a_close - (a_end + 1));
            }
          }
        }

        if (!call.empty() && !d1.empty()) {
          DXPedition de;
          de.call = call;
          de.location = loc;

          auto parseAdxoDate = [](const std::string &s, int &year, char *mon,
                                  int &day) -> bool {
            // Old format: "12 Jan 2026" — three space-separated tokens
            if (sscanf(s.c_str(), "%d %9s %d", &year, mon, &day) == 3)
              return true;
            // New format: "2026 Jan12" — year then Mmmdd concatenated
            char tmp[16] = {};
            if (sscanf(s.c_str(), "%d %15s", &year, tmp) == 2) {
              int i = 0;
              while (tmp[i] && !isdigit((unsigned char)tmp[i]))
                ++i;
              if (i > 0 && tmp[i]) {
                sscanf(tmp + i, "%d", &day);
                int copy = i < 9 ? i : 9;
                strncpy(mon, tmp, copy);
                mon[copy] = '\0';
                return day > 0;
              }
            }
            return false;
          };
          int y1, dy1, y2, dy2;
          char m1[10], m2[10];
          if (parseAdxoDate(d1, y1, m1, dy1) &&
              parseAdxoDate(d2, y2, m2, dy2)) {

            std::tm tm1 = {};
            tm1.tm_year = y1 - 1900;
            tm1.tm_mon = crackMonth(m1) - 1;
            tm1.tm_mday = dy1;
            de.startTime =
                std::chrono::system_clock::from_time_t(std::mktime(&tm1));

            std::tm tm2 = {};
            tm2.tm_year = y2 - 1900;
            tm2.tm_mon = crackMonth(m2) - 1;
            tm2.tm_mday = dy2;
            tm2.tm_hour = 23;
            tm2.tm_min = 59;
            de.endTime =
                std::chrono::system_clock::from_time_t(std::mktime(&tm2));

            auto yesterday = now - std::chrono::hours(24);
            if (de.endTime > yesterday) {
              update->dxpeds.push_back(de);
            }
          }
        }
        pos += 16;
      }

      if (update->dxpeds.empty())
        LOG_W("ActivityProvider",
              "fetchDXPeds: parsed 0 entries from non-empty HTTP response — "
              "ng3k.com HTML layout may have changed");

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_ACTIVITY_DATA_READY;
      event.user.code = static_cast<int>(UpdateType::DXPeds);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}

void ActivityProvider::fetchPOTA() {
  net_.fetchAsync(POTA_API_URL, [](std::string data) {
    if (data.empty())
      return;

    WorkerService::getInstance().submitTask([data]() {
      try {
        auto j = nlohmann::json::parse(data);
        if (!j.is_array())
          return;

        auto *update = new ActivityData();

        for (const auto &spot : j) {
          ONTASpot os;
          os.program = "POTA";
          os.call = spot.value("activator", "");
          os.ref = spot.value("reference", "");
          os.mode = spot.value("mode", "");
          std::string freq = spot.value("frequency", "0");
          os.freqKhz = StringUtils::safe_stod(freq) * 1000.0;
          os.spottedAt = std::chrono::system_clock::now();

          if (spot.contains("latitude")) {
            if (spot["latitude"].is_number())
              os.lat = spot["latitude"];
            else if (spot["latitude"].is_string())
              os.lat = StringUtils::safe_stod(spot["latitude"]);
          }
          if (spot.contains("longitude")) {
            if (spot["longitude"].is_number())
              os.lon = spot["longitude"];
            else if (spot["longitude"].is_string())
              os.lon = StringUtils::safe_stod(spot["longitude"]);
          }

          // Fallback to Manager or Grid Square if Lat/Lon missing
          if (os.lat == 0.0 && os.lon == 0.0) {
            float plat, plon;
            if (ActivityLocationManager::getInstance().getPOTALocation(
                    os.ref, plat, plon)) {
              os.lat = plat;
              os.lon = plon;
            } else {
              std::string grid = spot.value("grid", "");
              if (grid.empty())
                grid = spot.value("activatorGrid", "");
              if (!grid.empty()) {
                double glat, glon;
                if (Astronomy::gridToLatLon(grid, glat, glon)) {
                  os.lat = glat;
                  os.lon = glon;
                }
              }
            }
          }

          if (!os.call.empty()) {
            update->ontaSpots.push_back(os);
          }
        }

        SDL_Event event;
        SDL_zero(event);
        event.type = HamClock::AE_BASE_EVENT + HamClock::AE_ACTIVITY_DATA_READY;
        event.user.code = static_cast<int>(UpdateType::POTA);
        event.user.data1 = update;
        SDL_PushEvent(&event);
        LOG_I("ActivityProvider", "Offloaded POTA update with {} spots.",
              update->ontaSpots.size());

      } catch (const std::exception &e) {
        LOG_E("ActivityProvider", "POTA Parse Exception: {}", e.what());
      } catch (...) {
        LOG_E("ActivityProvider", "POTA Unknown Parse Exception");
      }
    });
  });
}

void ActivityProvider::fetchSOTA() {
  net_.fetchAsync(SOTA_API_URL, [](std::string data) {
    if (data.empty())
      return;

    WorkerService::getInstance().submitTask([data]() {
      try {
        auto j = nlohmann::json::parse(data);
        if (!j.is_array())
          return;

        auto *update = new ActivityData();

        for (const auto &spot : j) {
          ONTASpot os;
          os.program = "SOTA";
          os.call = spot.value("activatorCallsign", "");
          os.ref = spot.value("associationCode", "") + "/" +
                   spot.value("summitCode", "");
          os.mode = spot.value("mode", "");
          std::string freq = spot.value("frequency", "0");
          os.freqKhz = StringUtils::safe_stod(freq) *
                       1000.0; // SOTA MHz to kHz? Check API
          os.spottedAt = std::chrono::system_clock::now();

          float slat, slon;
          if (ActivityLocationManager::getInstance().getSOTALocation(
                  os.ref, slat, slon)) {
            os.lat = slat;
            os.lon = slon;
          } else {
            // Only trigger API lookup for well-formed SOTA refs (ASSOC/REGION-NNN
            // where the summit number is all digits, ruling out placeholders like
            // CS-0XX or CS-??X that the upstream API occasionally emits).
            auto slash = os.ref.find('/');
            auto dash  = os.ref.rfind('-');
            bool valid = (slash != std::string::npos && dash != std::string::npos &&
                          dash > slash && dash + 1 < os.ref.size());
            if (valid) {
              for (size_t i = dash + 1; i < os.ref.size(); ++i)
                if (!std::isdigit(static_cast<unsigned char>(os.ref[i])))
                  { valid = false; break; }
            }
            if (valid)
              ActivityLocationManager::getInstance().resolveSummitAsync(os.ref);
          }

          if (!os.call.empty()) {
            update->ontaSpots.push_back(os);
          }
        }

        SDL_Event event;
        SDL_zero(event);
        event.type = HamClock::AE_BASE_EVENT + HamClock::AE_ACTIVITY_DATA_READY;
        event.user.code = static_cast<int>(UpdateType::SOTA);
        event.user.data1 = update;
        SDL_PushEvent(&event);
        LOG_I("ActivityProvider", "Offloaded SOTA update with {} spots.",
              update->ontaSpots.size());

      } catch (...) {
        LOG_E("ActivityProvider", "SOTA Parse Error");
      }
    });
  });
}
