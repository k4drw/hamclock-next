#include "DRAPProvider.h"

DRAPProvider::DRAPProvider(NetworkManager &net) : net_(net) {}

void DRAPProvider::fetch(DataCb cb) {
  const char *url = "https://services.swpc.noaa.gov/images/animations/d-rap/"
                    "global/d-rap_global_latest.png";

  net_.fetchAsync(url, [cb](std::string body) {
    if (!body.empty()) {
      cb(body);
    }
  });
}
