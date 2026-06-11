#include "SDOProvider.h"
#include "../core/Astronomy.h"
#include <SDL.h>
#include <cstdio>
#include <ctime>
#include <regex>

SDOProvider::SDOProvider(NetworkManager &net) : net_(net) {}

void SDOProvider::fetch(const std::string &wavelength, bool pfss, DataCb cb) {
  lastFetchMs_ = SDL_GetTicks();
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

void SDOProvider::fetchMovieUrls(const std::string &wavelength, bool pfss, MovieUrlsCb cb) {
  std::time_t now = std::time(nullptr);
  std::tm utcToday{};
  Astronomy::portable_gmtime(&now, &utcToday);
  
  std::time_t yesterday = now - 86400;
  std::tm utcYesterday{};
  Astronomy::portable_gmtime(&yesterday, &utcYesterday);
  
  char urlYest[256];
  std::snprintf(urlYest, sizeof(urlYest), "https://sdo.gsfc.nasa.gov/assets/img/browse/%04d/%02d/%02d/", 
      utcYesterday.tm_year + 1900, utcYesterday.tm_mon + 1, utcYesterday.tm_mday);
      
  char urlToday[256];
  std::snprintf(urlToday, sizeof(urlToday), "https://sdo.gsfc.nasa.gov/assets/img/browse/%04d/%02d/%02d/", 
      utcToday.tm_year + 1900, utcToday.tm_mon + 1, utcToday.tm_mday);

  std::string syest = urlYest;
  std::string stoday = urlToday;

  std::weak_ptr<SDOProvider> self = shared_from_this();
  net_.fetchAsync(syest, [self, cb, syest, stoday, wavelength, pfss](std::string bodyYest) {
    auto p1 = self.lock();
    if (!p1) return;
    p1->net_.fetchAsync(stoday, [self, cb, syest, stoday, wavelength, pfss, bodyYest](std::string bodyToday) {
      auto p2 = self.lock();
      if (!p2) return;
      
      std::string targetWavelength = wavelength;
      if (wavelength == "211193171") targetWavelength = pfss ? "211_193_171pfss" : "211_193_171";
      else if (wavelength == "HMIB") targetWavelength = "HMIB";
      else if (wavelength == "HMIIC") targetWavelength = "HMIIC";
      else if (pfss) targetWavelength = wavelength + "pfss";

      std::string patternStr = ">(\\d{8}_\\d{6}_512_" + targetWavelength + "\\.jpg)</a>";
      std::regex re(patternStr);

      std::vector<std::string> matches;
      auto extract = [&](const std::string& html, const std::string& base) {
        auto begin = std::sregex_iterator(html.begin(), html.end(), re);
        auto end = std::sregex_iterator();
        for (std::sregex_iterator i = begin; i != end; ++i) {
          matches.push_back(base + i->str(1));
        }
      };

      extract(bodyYest, syest);
      extract(bodyToday, stoday);

      std::vector<std::string> finalUrls;
      if (!matches.empty()) {
        int count = matches.size();
        size_t frames = 24;
        int step = 5; // Every 4th image = ~1 hour interval
        int startIndex = count - 1 - ((frames - 1) * step);
        if (startIndex < 0) {
          step = 1;
          startIndex = count - frames;
          if (startIndex < 0) startIndex = 0;
        }
        for (int i = startIndex; i < count; i += step) {
          finalUrls.push_back(matches[i]);
          if (finalUrls.size() >= frames) break;
        }
      }

      cb(finalUrls);

    }, 3600, false);
  }, 3600, false);
}
