#include "LoTWProvider.h"
#include "../core/Logger.h"
#include <SDL.h>
#include <ctime>

LoTWProvider::LoTWProvider(NetworkManager &net, std::shared_ptr<ADIFStore> store,
                           const std::string &call, const std::string &password)
    : net_(net), store_(store), call_(call), password_(password) {
  auto now = std::time(nullptr);
  auto tm = std::gmtime(&now);
  char datebuf[16];
  std::strftime(datebuf, sizeof(datebuf), "%Y%m%d", tm);
  lastSyncDate_ = datebuf;
}

void LoTWProvider::setCredentials(const std::string &call,
                                   const std::string &password) {
  call_ = call;
  password_ = password;
}

void LoTWProvider::fetch() {
  if (call_.empty() || password_.empty()) {
    LOG_W("LoTWProvider", "No credentials configured");
    return;
  }

  lastFetchMs_ = SDL_GetTicks();

  std::string url = std::string(LOTW_API_URL) +
                    "?login=" + call_ + "&password=" + password_ +
                    "&qso_query=1&qso_qslsince=" + lastSyncDate_;

  std::weak_ptr<LoTWProvider> weakSelf = shared_from_this();
  net_.fetchAsync(url, [weakSelf](std::string data) {
    auto self = weakSelf.lock();
    if (!self)
      return;
    if (data.empty()) {
      LOG_E("LoTWProvider", "Failed to fetch LoTW confirmations");
      return;
    }

    // Parse ADIF and merge into store
    // For now, log successful fetch - full mergeEntries implementation deferred
    LOG_I("LoTWProvider", "Received %zu bytes of LoTW ADIF data",
          data.length());
  });
}
