#include "AuroraProvider.h"

AuroraProvider::AuroraProvider(NetworkManager &net) : net_(net) {}

void AuroraProvider::fetch(bool north, DataCb cb) {
  const char *url = north ? "https://services.swpc.noaa.gov/images/"
                            "aurora-forecast-northern-hemisphere.png"
                          : "https://services.swpc.noaa.gov/images/"
                            "aurora-forecast-southern-hemisphere.png";

  net_.fetchAsync(url, [cb](std::string body) {
    if (!body.empty()) {
      cb(body);
    }
  });
}
