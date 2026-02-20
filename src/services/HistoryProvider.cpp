#include "HistoryProvider.h"
#include "../core/Astronomy.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>

HistoryProvider::HistoryProvider(NetworkManager &net,
                                 std::shared_ptr<HistoryStore> store)
    : net_(net), store_(std::move(store)) {}

void HistoryProvider::fetchFlux() {
  net_.fetchAsync(FLUX_URL, [](std::string body) {
    if (body.empty())
      return;
    WorkerService::getInstance().submitTask([body]() {
      HistorySeries *update = new HistorySeries();
      update->name = "flux";

      std::stringstream ss(body);
      std::string line;
      std::vector<HistoryPoint> points;

      while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ':')
          continue;

        int y, m, d, ssn, flux;
        if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &ssn,
                        &flux) == 5) {
          struct tm t = {0};
          t.tm_year = y - 1900;
          t.tm_mon = m - 1;
          t.tm_mday = d;
          points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                            Astronomy::portable_timegm(&t)),
                                        (float)flux));
        }
      }

      if (points.size() > 30)
        points.erase(points.begin(), points.end() - 30);

      update->points = points;
      if (!points.empty()) {
        update->minValue = points[0].value;
        update->maxValue = points[0].value;
        for (const auto &p : points) {
          update->minValue = std::min(update->minValue, p.value);
          update->maxValue = std::max(update->maxValue, p.value);
        }
        update->valid = true;
      }

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_HISTORY_DATA_READY;
      event.user.code = static_cast<int>(SeriesType::Flux);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}

void HistoryProvider::fetchSSN() {
  net_.fetchAsync(FLUX_URL, [](std::string body) {
    if (body.empty())
      return;
    WorkerService::getInstance().submitTask([body]() {
      HistorySeries *update = new HistorySeries();
      update->name = "ssn";

      std::stringstream ss(body);
      std::string line;
      std::vector<HistoryPoint> points;

      while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ':')
          continue;

        int y, m, d, ssn, flux;
        if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &ssn,
                        &flux) == 5) {
          struct tm t = {0};
          t.tm_year = y - 1900;
          t.tm_mon = m - 1;
          t.tm_mday = d;
          points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                            Astronomy::portable_timegm(&t)),
                                        (float)ssn));
        }
      }

      if (points.size() > 30)
        points.erase(points.begin(), points.end() - 30);
      update->points = points;
      if (!points.empty()) {
        update->minValue = points[0].value;
        update->maxValue = points[0].value;
        for (const auto &p : points) {
          update->minValue = std::min(update->minValue, p.value);
          update->maxValue = std::max(update->maxValue, p.value);
        }
        update->valid = true;
      }

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_HISTORY_DATA_READY;
      event.user.code = static_cast<int>(SeriesType::SSN);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}

void HistoryProvider::fetchKp() {
  net_.fetchAsync(KP_URL, [](std::string body) {
    if (body.empty())
      return;
    WorkerService::getInstance().submitTask([body]() {
      HistorySeries *update = new HistorySeries();
      update->name = "kp";

      std::stringstream ss(body);
      std::string line;
      std::vector<HistoryPoint> points;

      while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ':')
          continue;

        int y, m, d, aIndex, kIndex;
        if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &aIndex,
                        &kIndex) == 5) {
          struct tm t = {0};
          t.tm_year = y - 1900;
          t.tm_mon = m - 1;
          t.tm_mday = d;
          points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                            Astronomy::portable_timegm(&t)),
                                        (float)kIndex));
        }
      }

      if (points.size() > 30)
        points.erase(points.begin(), points.end() - 30);
      update->points = points;
      if (!points.empty()) {
        update->valid = true;
        update->minValue = 0;
        update->maxValue = 9; // Kp is 0-9
      }

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_HISTORY_DATA_READY;
      event.user.code = static_cast<int>(SeriesType::Kp);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}
