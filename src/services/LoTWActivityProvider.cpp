#include "LoTWActivityProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

LoTWActivityProvider::LoTWActivityProvider(NetworkManager &net,
                                           std::shared_ptr<LoTWActivityStore> store)
    : net_(net), store_(store) {}

void LoTWActivityProvider::fetch() {
  lastFetchMs_ = SDL_GetTicks();
  net_.fetchAsync(LOTW_ACTIVITY_URL, [this](std::string data) {
    if (data.empty()) {
      LOG_E("LoTWActivityProvider", "Failed to fetch LoTW activity CSV");
      return;
    }

    auto store = store_;
    WorkerService::getInstance().submitTask([store, data]() {
      std::unordered_map<std::string, std::chrono::system_clock::time_point>
          activity;
      std::istringstream stream(data);
      std::string line;
      int droppedLines = 0;

      // Skip header line if present
      bool isFirstLine = true;

      while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty())
          continue;

        // Skip header (first line if it contains "Call")
        if (isFirstLine) {
          isFirstLine = false;
          if (line.find("Call") != std::string::npos ||
              line.find("call") != std::string::npos)
            continue;
        }

        // Parse CSV: callsign,upload_date
        // Expected format: W5XYZ,2026-04-23
        size_t commaPos = line.find(',');
        if (commaPos == std::string::npos) {
          ++droppedLines;
          continue;
        }

        std::string call = line.substr(0, commaPos);
        std::string dateStr = line.substr(commaPos + 1);

        // Trim whitespace
        call = StringUtils::trim(call);
        dateStr = StringUtils::trim(dateStr);

        // Parse date YYYY-MM-DD
        // Use simple string parsing: 2026-04-23
        struct tm tm = {};
        int year = 0, month = 0, day = 0;
        if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
          tm.tm_year = year - 1900;
          tm.tm_mon = month - 1;
          tm.tm_mday = day;
          tm.tm_hour = 0;
          tm.tm_min = 0;
          tm.tm_sec = 0;
          tm.tm_isdst = -1;

          time_t t = mktime(&tm);
          if (t != -1) {
            // Convert call to uppercase
            std::string callUpper = call;
            std::transform(callUpper.begin(), callUpper.end(),
                          callUpper.begin(),
                          [](unsigned char c) { return std::toupper(c); });
            activity[callUpper] =
                std::chrono::system_clock::from_time_t(t);
          } else {
            ++droppedLines;
          }
        } else {
          ++droppedLines;
        }
      }

      if (!activity.empty()) {
        store->update(activity);
        LOG_I("LoTWActivityProvider", "Loaded %zu LoTW user records",
              activity.size());
      } else {
        LOG_W("LoTWActivityProvider",
              "No LoTW activity records parsed from CSV");
      }

      if (droppedLines > 0) {
        LOG_W("LoTWActivityProvider", "Dropped %d malformed lines", droppedLines);
      }
    });
  });
}
