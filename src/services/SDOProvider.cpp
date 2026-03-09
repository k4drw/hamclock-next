#include "SDOProvider.h"
#include "../core/Astronomy.h"
#include <cstdio>
#include <ctime>

SDOProvider::SDOProvider(NetworkManager &net) : net_(net) {}

void SDOProvider::fetch(const std::string &wavelength, bool pfss, DataCb cb) {
  // 1. Prepare LMSAL URL (Now Primary)
  std::string primaryUrl;
  if (wavelength == "211193171") {
    primaryUrl = "http://sdowww.lmsal.com/sdomedia/SunInTime/mostrecent/"
                 "l_211_193_171.jpg";
  } else if (wavelength == "HMIIC") {
    // HMI requires date-based URL on LMSAL
    std::time_t now = std::time(nullptr);
    std::tm utc{};
    Astronomy::portable_gmtime(&now, &utc);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "https://suntoday.lmsal.com/sdomedia/SunInTime/%04d/%02d/"
                  "%02d/l_HMI_cont_aiascale.jpg",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
    primaryUrl = buf;
  } else if (wavelength == "HMIB") {
    // Magnetogram with PFSS lines requires date-based URL
    std::time_t now = std::time(nullptr);
    std::tm utc{};
    Astronomy::portable_gmtime(&now, &utc);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "https://suntoday.lmsal.com/sdomedia/SunInTime/%04d/%02d/"
                  "%02d/f_HMImagpfss.jpg",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
    primaryUrl = buf;
  } else {
    if (pfss) {
      // PFSS AIA wavelengths require date-based URL
      std::time_t now = std::time(nullptr);
      std::tm utc{};
      Astronomy::portable_gmtime(&now, &utc);
      char buf[256];
      std::snprintf(buf, sizeof(buf),
                    "https://suntoday.lmsal.com/sdomedia/SunInTime/%04d/%02d/"
                    "%02d/f%spfss.jpg",
                    utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                    wavelength.c_str());
      primaryUrl = buf;
    } else {
      primaryUrl = "http://sdowww.lmsal.com/sdomedia/SunInTime/mostrecent/l" +
                   wavelength + ".jpg";
    }
  }

  // 2. Prepare NASA URL (Now Backup)
  char nasa[256];
  std::snprintf(nasa, sizeof(nasa),
                "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_512_%s.jpg",
                wavelength.c_str());
  std::string nasaUrl(nasa);

  // 3. Prepare sol24.net URL (Tertiary Backup)
  char sol24[256];
  std::snprintf(sol24, sizeof(sol24),
                "https://sol24.net/data/latest_4096_%s.jpg",
                wavelength.c_str());
  std::string sol24Url(sol24);

  // Start Fetch Chain: LMSAL -> NASA -> sol24
  std::weak_ptr<SDOProvider> self = shared_from_this();
  net_.fetchAsync(
      primaryUrl,
      [self, cb, primaryUrl, nasaUrl, sol24Url](std::string body) {
        auto p = self.lock();
        if (!p) return;
        if (!body.empty()) {
          cb(body, p->net_.getCacheServerTime(primaryUrl));
          return;
        }

        // LMSAL failed — try NASA
        p->net_.fetchAsync(
            nasaUrl,
            [self, cb, nasaUrl, sol24Url](std::string body2) {
              auto p2 = self.lock();
              if (!p2) return;
              if (!body2.empty()) {
                cb(body2, p2->net_.getCacheServerTime(nasaUrl));
                return;
              }
              // NASA failed — try sol24.net
              p2->net_.fetchAsync(
                  sol24Url,
                  [self, cb, sol24Url](std::string body3) {
                    auto p3 = self.lock();
                    if (!p3) return;
                    if (!body3.empty()) {
                      cb(body3, p3->net_.getCacheServerTime(sol24Url));
                    }
                  },
                  3600, false);
            },
            3600, false);
      },
      3600, false);
}
