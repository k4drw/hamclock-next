#include "SDOProvider.h"
#include <cstdio>

SDOProvider::SDOProvider(NetworkManager &net) : net_(net) {}

void SDOProvider::fetch(const std::string &wavelength, DataCb cb) {
  char url[256];
  std::snprintf(url, sizeof(url),
                "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_512_%s.jpg",
                wavelength.c_str());

  net_.fetchAsync(url, [cb](std::string body) {
    if (!body.empty()) {
      cb(body);
    }
  });
}
