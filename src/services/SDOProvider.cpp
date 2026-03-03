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
  net_.fetchAsync(
      primaryUrl,
      [this, cb, primaryUrl, nasaUrl, sol24Url](std::string body) {
        if (!body.empty()) {
          cb(body, net_.getCacheServerTime(primaryUrl));
          return;
        }

        // LMSAL failed — try NASA
        net_.fetchAsync(
            nasaUrl,
            [this, cb, nasaUrl, sol24Url](std::string body2) {
              if (!body2.empty()) {
                cb(body2, net_.getCacheServerTime(nasaUrl));
                return;
              }
              // NASA failed — try sol24.net
              net_.fetchAsync(
                  sol24Url,
                  [this, cb, sol24Url](std::string body3) {
                    if (!body3.empty()) {
                      cb(body3, net_.getCacheServerTime(sol24Url));
                    }
                  },
                  3600, false);
            },
            3600, false);
      },
      3600, false);
}
