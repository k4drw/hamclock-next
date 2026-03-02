#include "SDOProvider.h"
#include <cstdio>

SDOProvider::SDOProvider(NetworkManager &net) : net_(net) {}

void SDOProvider::fetch(const std::string &wavelength, DataCb cb) {
  char primary[256];
  std::snprintf(primary, sizeof(primary),
                "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_512_%s.jpg",
                wavelength.c_str());

  char fallback[256];
  std::snprintf(fallback, sizeof(fallback),
                "https://sol24.net/data/latest_4096_%s.jpg",
                wavelength.c_str());

  std::string fallbackUrl(fallback);
  net_.fetchAsync(primary, [this, cb, fallbackUrl](std::string body) {
    if (!body.empty()) {
      cb(body);
      return;
    }
    // Primary failed — try sol24.net fallback
    net_.fetchAsync(fallbackUrl, [cb](std::string body2) {
      if (!body2.empty()) {
        cb(body2);
      }
    }, /*cacheAgeSeconds=*/3600, /*force=*/false);
  });
}
