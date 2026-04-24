#include "ClublogProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

ClublogProvider::ClublogProvider(NetworkManager &net,
                                 std::shared_ptr<ClublogStore> store,
                                 const std::string &apiKey)
    : net_(net), store_(store), apiKey_(apiKey) {}

void ClublogProvider::fetch() {
  if (apiKey_.empty()) {
    LOG_W("ClublogProvider", "No API key configured");
    return;
  }

  lastFetchMs_ = SDL_GetTicks();

  std::string url = std::string(CLUBLOG_API_BASE) + "?api=" + apiKey_ +
                    "&format=json&mode=dx";

  net_.fetchAsync(url, [this](std::string data) {
    if (data.empty()) {
      LOG_E("ClublogProvider", "Failed to fetch Clublog most wanted");
      return;
    }

    WorkerService::getInstance().submitTask([this, data]() {
      try {
        auto j = json::parse(data);
        std::vector<MostWantedEntry> entries;

        if (j.is_array()) {
          for (const auto &item : j) {
            MostWantedEntry entry;
            entry.rank = item.value("rank", 0);
            entry.dxcc = item.value("dxcc", 0);
            entry.name = item.value("name", "");
            entry.prefix = item.value("prefix", "");
            if (entry.dxcc > 0 && !entry.name.empty())
              entries.push_back(entry);
          }
        }

        if (!entries.empty()) {
          store_->update(entries);
          LOG_I("ClublogProvider", "Loaded %zu most wanted DXCC entities",
                entries.size());
        } else {
          LOG_W("ClublogProvider", "No valid entries parsed from JSON");
        }
      } catch (const std::exception &e) {
        LOG_E("ClublogProvider", "JSON parse error: %s", e.what());
      }
    });
  });
}
