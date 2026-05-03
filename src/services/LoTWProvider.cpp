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
    if (statusCallback_) {
      statusCallback_(0, 0, "No credentials configured");
    }
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

    time_t now = std::time(nullptr);
    int qsoCount = 0;
    std::string error;

    if (data.empty()) {
      error = "Failed to fetch LoTW data";
      LOG_E("LoTWProvider", "%s", error.c_str());
    } else {
      qsoCount = std::count(data.begin(), data.end(), '\n') / 15;
      LOG_I("LoTWProvider", "Received %zu bytes of LoTW ADIF data with ~%d QSOs",
            data.length(), qsoCount);
    }

    if (self->statusCallback_) {
      self->statusCallback_(error.empty() ? now : 0, qsoCount, error);
    }
  });
}
