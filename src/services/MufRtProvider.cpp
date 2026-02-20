#include "MufRtProvider.h"
#include <SDL.h>

MufRtProvider::MufRtProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

void MufRtProvider::update() {
  uint32_t now = SDL_GetTicks();
  // Update every 30 minutes (1800000 ms)
  if (hasData_ && (now - lastUpdateMs_ < 1800000)) {
    return;
  }

  // Native PropEngine now provides MUF overlay updates.
  // No external fetch required.
}

bool MufRtProvider::hasData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return hasData_;
}

const std::string &MufRtProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pngData_;
}
