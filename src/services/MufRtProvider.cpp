#include "MufRtProvider.h"
#include <SDL.h>

MufRtProvider::MufRtProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

void MufRtProvider::update() {
  // MUF-RT is now handled internally by PropEngine using data
  // from IonosondeProvider (https://prop.kc2g.com/api/stations.json).
  // No external image fetch is required.
}

bool MufRtProvider::hasData() const { return false; }

const std::string &MufRtProvider::getData() const { return pngData_; }
