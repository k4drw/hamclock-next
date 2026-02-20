#include "CloudProvider.h"
#include <SDL.h>

CloudProvider::CloudProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

void CloudProvider::update() {
  // Cloud overlay is temporarily disabled pending full planning.
}

bool CloudProvider::hasData() const {
  return false;
}

const std::string &CloudProvider::getData() const {
  static const std::string empty;
  return empty;
}
