#include "LoTWActivityProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/TimeUtils.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>

LoTWActivityProvider::LoTWActivityProvider(NetworkManager &net,
                                           std::shared_ptr<LoTWActivityStore> store)
    : net_(net), store_(store) {}

void LoTWActivityProvider::fetch() {
  lastFetchMs_ = SDL_GetTicks();
  std::weak_ptr<LoTWActivityProvider> weakSelf = shared_from_this();

  net_.fetchAsync(LOTW_ACTIVITY_URL, [weakSelf](std::string data) {
    auto self = weakSelf.lock();
    if (!self) {
      LOG_D("LoTWActivityProvider", "Callback fired after provider destruction, ignoring");
      return;
    }
    if (data.empty()) {
      LOG_E("LoTWActivityProvider", "Failed to fetch LoTW activity CSV");
      return;
    }

    auto store = self->store_;
    WorkerService::getInstance().submitTask([store, data = std::move(data)]() {
      std::unordered_map<std::string, std::chrono::system_clock::time_point>
          activity;
      std::string_view sv(data);
      int droppedLines = 0;
      bool isFirstLine = true;

      while (!sv.empty()) {
        size_t pos = sv.find('\n');
        std::string_view line = (pos == std::string_view::npos) ? sv : sv.substr(0, pos);
        if (pos == std::string_view::npos) sv = {};
        else sv.remove_prefix(pos + 1);

        if (line.empty())
          continue;

        if (isFirstLine) {
          isFirstLine = false;
          if (line.find("Call") != std::string_view::npos ||
              line.find("call") != std::string_view::npos)
            continue;
        }

        size_t commaPos = line.find(',');
        if (commaPos == std::string_view::npos) {
          ++droppedLines;
          continue;
        }

        std::string_view call = line.substr(0, commaPos);
        std::string_view dateStr = line.substr(commaPos + 1);

        call = StringUtils::trimSV(call);
        dateStr = StringUtils::trimSV(dateStr);

        time_t t = TimeUtils::isoToTimeT(dateStr);
        if (t > 0) {
          std::string callUpper(call);
          std::transform(callUpper.begin(), callUpper.end(),
                        callUpper.begin(),
                        [](unsigned char c) { return std::toupper(c); });
          activity[callUpper] =
              std::chrono::system_clock::from_time_t(t);
        } else {
          ++droppedLines;
        }
      }

      if (!activity.empty()) {
        size_t count = activity.size();
        store->update(std::move(activity));
        LOG_I("LoTWActivityProvider", "Loaded {} LoTW user records", (int)count);
      } else {
        LOG_W("LoTWActivityProvider", "No LoTW activity records parsed from CSV");
      }

      if (droppedLines > 0) {
        LOG_W("LoTWActivityProvider", "Dropped {} malformed lines", droppedLines);
      }
    });
  });
}
