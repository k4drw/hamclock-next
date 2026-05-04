#include "LoTWProvider.h"
#include "../core/Astronomy.h"
#include "../core/Logger.h"
#include "../core/PrefixManager.h"
#include "../core/StringUtils.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>
#include <fstream>

LoTWProvider::LoTWProvider(NetworkManager &net, std::shared_ptr<ADIFStore> store,
                           PrefixManager &prefixMgr,
                           const std::string &call, const std::string &password)
    : net_(net), store_(store), prefixMgr_(prefixMgr), call_(call), password_(password) {
  time_t now = std::time(nullptr);
  now -= 30 * 24 * 60 * 60;
  auto tm = std::gmtime(&now);
  char datebuf[16];
  std::strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", tm);
  lastSyncDate_ = datebuf;
}

void LoTWProvider::setCredentials(const std::string &call,
                                   const std::string &password) {
  call_ = call;
  password_ = password;
}

static std::string getTagContent(const std::string &record, const std::string &tag) {
  std::string upperRecord = record;
  std::string upperTag = tag;
  std::transform(upperRecord.begin(), upperRecord.end(), upperRecord.begin(), ::toupper);
  std::transform(upperTag.begin(), upperTag.end(), upperTag.begin(), ::toupper);

  size_t pos = upperRecord.find("<" + upperTag + ":");
  if (pos == std::string::npos) {
    pos = upperRecord.find("<" + upperTag + ">");
    if (pos != std::string::npos) {
      size_t start = pos + upperTag.length() + 2;
      size_t end = upperRecord.find("<", start);
      if (end != std::string::npos) {
        return record.substr(start, end - start);
      }
      return record.substr(start);
    }
    return "";
  }

  size_t colon = record.find(":", pos);
  if (colon == std::string::npos) return "";
  size_t typeStart = colon + 1;

  size_t nextColon = record.find(":", typeStart);
  size_t close = record.find(">", typeStart);

  if (close == std::string::npos)
    return "";

  int len = 0;
  try {
    std::string lenStr;
    if (nextColon != std::string::npos && nextColon < close) {
      lenStr = record.substr(typeStart, nextColon - typeStart);
    } else {
      lenStr = record.substr(typeStart, close - typeStart);
    }
    len = std::stoi(lenStr);
  } catch (...) {
    return "";
  }

  size_t valueStart = close + 1;
  if (valueStart + len > record.length()) {
    len = record.length() - valueStart;
  }
  if (len > 1024) len = 1024;

  return record.substr(valueStart, len);
}

static std::string urlEncode(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : value) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
      escaped << std::nouppercase;
    }
  }

  return escaped.str();
}

void LoTWProvider::fetch() {
  if (call_.empty() || password_.empty()) {
    LOG_W("LoTWProvider", "No credentials configured");
    if (statusCallback_) {
      statusCallback_(0, 0, "No credentials configured");
    }
    return;
  }

  lastFetchMs_ = SDL_GetTicks();

  std::string url = std::string(LOTW_API_URL) +
                    "?login=" + urlEncode(call_) + "&password=" + urlEncode(password_) +
                    "&qso_query=1&qso_qsl=yes&qso_qslsince=" + lastSyncDate_;


  std::weak_ptr<LoTWProvider> weakSelf = shared_from_this();
  net_.fetchAsync(url, [weakSelf](std::string data) {
    auto self = weakSelf.lock();
    if (!self)
      return;

    std::ifstream f("/home/ziggy/tmp/hc-working/hamclock-next/lotwreport.adi");
    if (f.is_open()) {
      std::stringstream buf;
      buf << f.rdbuf();
      data = buf.str();
    }

    time_t now = std::time(nullptr);
    int qsoCount = 0;
    std::string error;

    std::string upperData = data;
    std::transform(upperData.begin(), upperData.end(), upperData.begin(), ::toupper);

    if (data.empty() || (upperData.find("<EOH>") == std::string::npos && upperData.find("<EOR>") == std::string::npos)) {
      error = "Failed to fetch LoTW data or invalid credentials";
      LOG_E("LoTWProvider", "%s", error.c_str());
    } else {
      // Valid data received, parse and update existing store
      ADIFStats stats = self->store_ ? self->store_->get() : ADIFStats{};

      // Find `<EOH>` and skip past it
      size_t eohPos = upperData.find("<EOH>");
      std::string recordsStr = (eohPos != std::string::npos) ? data.substr(eohPos + 5) : data;

      // Extract records using `<EOR>` tag
      size_t start = 0;
      size_t end = 0;
      while ((end = upperData.find("<EOR>", start)) != std::string::npos) {
        std::string record = data.substr(start, end - start);
        start = end + 5; // length of "<EOR>"

        std::string call = getTagContent(record, "CALL");
        if (call.empty()) continue;

        std::string mode = getTagContent(record, "MODE");
        std::string band = getTagContent(record, "BAND");
        std::string freq = getTagContent(record, "FREQ");
        std::string qsoDate = getTagContent(record, "QSO_DATE");
        std::string timeOn = getTagContent(record, "TIME_ON");
        std::string rstSent = getTagContent(record, "RST_SENT");
        std::string rstRcvd = getTagContent(record, "RST_RCVD");
        std::string state = getTagContent(record, "STATE");
        std::string cqZone = getTagContent(record, "CQZ");
        std::string ituZone = getTagContent(record, "ITUZ");
        std::string gridsquare = getTagContent(record, "GRIDSQUARE");
        std::string name = getTagContent(record, "NAME");
        std::string qth = getTagContent(record, "QTH");
        std::string comment = getTagContent(record, "COMMENT");
        std::string latStr = getTagContent(record, "LAT");
        std::string lonStr = getTagContent(record, "LON");

        stats.totalQSOs++;
        if (!mode.empty()) {
          stats.modeCounts[mode]++;
        }

        std::string useBand = band;
        if (useBand.empty() && !freq.empty()) {
          double freqMhz = StringUtils::safe_stod(freq);
          if (freqMhz >= 1.8 && freqMhz < 2.0) useBand = "160m";
          else if (freqMhz >= 3.5 && freqMhz < 4.0) useBand = "80m";
          else if (freqMhz >= 7.0 && freqMhz < 7.3) useBand = "40m";
          else if (freqMhz >= 10.1 && freqMhz < 10.15) useBand = "30m";
          else if (freqMhz >= 14.0 && freqMhz < 14.35) useBand = "20m";
          else if (freqMhz >= 18.068 && freqMhz < 18.168) useBand = "17m";
          else if (freqMhz >= 21.0 && freqMhz < 21.45) useBand = "15m";
          else if (freqMhz >= 24.89 && freqMhz < 24.99) useBand = "12m";
          else if (freqMhz >= 28.0 && freqMhz < 29.7) useBand = "10m";
          else if (freqMhz >= 50.0 && freqMhz < 54.0) useBand = "6m";
          else if (freqMhz >= 144.0 && freqMhz < 148.0) useBand = "2m";
          else if (freqMhz >= 420.0 && freqMhz < 450.0) useBand = "70cm";
        }

        if (!useBand.empty()) {
          stats.bandCounts[useBand]++;
        }

        int entityNum = self->prefixMgr_.findDXCC(call);
        if (entityNum > 0) {
          stats.workedEntitiesPerBand[entityNum].insert(useBand);
          stats.confirmedEntitiesPerBand[entityNum].insert(useBand);
        }

        if (!state.empty()) {
          std::string uState = state;
          std::transform(uState.begin(), uState.end(), uState.begin(), ::toupper);
          stats.workedStates.insert(uState);
          stats.confirmedStates.insert(uState);
        }

        if (entityNum > 0) {
          std::string cont = self->prefixMgr_.getContinent(entityNum);
          if (!cont.empty()) {
            stats.workedContinents.insert(cont);
            stats.confirmedContinents.insert(cont);
          }
        }

        if (!useBand.empty()) {
          if (!cqZone.empty()) {
            int z = StringUtils::safe_stoi(cqZone);
            if (z >= 1 && z <= 40) {
              stats.workedZonesCQ[z].insert(useBand);
              stats.confirmedZonesCQ[z].insert(useBand);
            }
          }
          if (!ituZone.empty()) {
            int z = StringUtils::safe_stoi(ituZone);
            if (z >= 1 && z <= 75) {
              stats.workedZonesITU[z].insert(useBand);
              stats.confirmedZonesITU[z].insert(useBand);
            }
          }
        }

        if (!gridsquare.empty()) {
          std::string grid4 = gridsquare;
          std::transform(grid4.begin(), grid4.end(), grid4.begin(), ::toupper);
          if (grid4.length() >= 4) {
            grid4 = grid4.substr(0, 4);
            stats.workedGrids4[grid4] = true;
            stats.confirmedGrids4[grid4] = true;
          }
        }

        auto it = std::find(stats.latestCalls.begin(), stats.latestCalls.end(), call);
        if (it != stats.latestCalls.end()) {
          stats.latestCalls.erase(it);
        }
        stats.latestCalls.insert(stats.latestCalls.begin(), call);
        if (stats.latestCalls.size() > 10) {
          stats.latestCalls.resize(10);
        }

        QSORecord qso;
        qso.callsign = call.length() > 16 ? call.substr(0, 16) : call;
        qso.date = qsoDate.length() > 16 ? qsoDate.substr(0, 16) : qsoDate;
        qso.time = timeOn.length() > 16 ? timeOn.substr(0, 16) : timeOn;
        qso.band = useBand.length() > 8 ? useBand.substr(0, 8) : useBand;
        qso.mode = mode.length() > 16 ? mode.substr(0, 16) : mode;
        qso.freq = freq.length() > 16 ? freq.substr(0, 16) : freq;
        qso.rstSent = rstSent.length() > 8 ? rstSent.substr(0, 8) : rstSent;
        qso.rstRcvd = rstRcvd.length() > 8 ? rstRcvd.substr(0, 8) : rstRcvd;
        qso.state = state.length() > 8 ? state.substr(0, 8) : state;
        qso.name = name.length() > 32 ? name.substr(0, 32) : name;
        qso.qth = qth.length() > 32 ? qth.substr(0, 32) : qth;
        qso.gridsquare = gridsquare.length() > 16 ? gridsquare.substr(0, 16) : gridsquare;
        qso.comment = comment.length() > 64 ? comment.substr(0, 64) : comment;

        if (!latStr.empty() && !lonStr.empty()) {
          qso.lat = StringUtils::safe_stod(latStr);
          qso.lon = StringUtils::safe_stod(lonStr);
        } else if (!gridsquare.empty()) {
          Astronomy::gridToLatLon(gridsquare.c_str(), qso.lat, qso.lon);
        } else {
          LatLong ll;
          if (self->prefixMgr_.findLocation(call, ll)) {
            qso.lat = ll.lat;
            qso.lon = ll.lon;
          }
        }

        stats.recentQSOs.insert(stats.recentQSOs.begin(), qso);
        if (stats.recentQSOs.size() > 100) {
          stats.recentQSOs.resize(100);
        }

        qsoCount++;
      }

      stats.valid = true;
      if (self->store_) {
        self->store_->update(stats);
      }

      // Update date since to today on success
      auto now_time = std::time(nullptr);
      auto tm = std::gmtime(&now_time);
      char datebuf[16];
      std::strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", tm);
      self->lastSyncDate_ = datebuf;

      LOG_I("LoTWProvider", "Received %zu bytes of LoTW ADIF data with ~%d QSOs",
            data.length(), qsoCount);
    }

    self->lastSyncTime_ = error.empty() ? now : 0;
    self->qsosSynced_ = qsoCount;
    self->lastError_ = error;

    if (self->statusCallback_) {
      self->statusCallback_(self->lastSyncTime_, self->qsosSynced_, self->lastError_);
    }
  }, 0, true);
}


