#include "HistoryProvider.h"
#include "../core/Astronomy.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>

HistoryProvider::HistoryProvider(NetworkManager &net,
                                 std::shared_ptr<HistoryStore> store)
    : net_(net), store_(std::move(store)) {}

void HistoryProvider::fetchFlux() {
  net_.fetchAsync(FLUX_URL, [this](std::string body) {
    if (!body.empty())
      processFlux(body);
  });
}

void HistoryProvider::fetchSSN() {
  net_.fetchAsync(FLUX_URL, [this](std::string body) {
    if (!body.empty())
      processSSN(body);
  });
}

void HistoryProvider::fetchKp() {
  net_.fetchAsync(KP_URL, [this](std::string body) {
    if (!body.empty())
      processKp(body);
  });
}

void HistoryProvider::processFlux(const std::string &body) {
  HistorySeries series;
  series.name = "flux";

  std::stringstream ss(body);
  std::string line;
  std::vector<HistoryPoint> points;

  while (std::getline(ss, line)) {
    if (line.empty() || line[0] == '#' || line[0] == ':')
      continue;

    int y, m, d, ssn, flux;
    if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &ssn, &flux) ==
        5) {
      struct tm t = {0};
      t.tm_year = y - 1900;
      t.tm_mon = m - 1;
      t.tm_mday = d;
      points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                        Astronomy::portable_timegm(&t)),
                                    (float)flux));
    }
  }

  // Only keep last 30
  if (points.size() > 30)
    points.erase(points.begin(), points.end() - 30);

  series.points = points;
  if (!points.empty()) {
    series.minValue = points[0].value;
    series.maxValue = points[0].value;
    for (const auto &p : points) {
      series.minValue = std::min(series.minValue, p.value);
      series.maxValue = std::max(series.maxValue, p.value);
    }
    series.valid = true;
  }
  store_->update("flux", series);
}

void HistoryProvider::processSSN(const std::string &body) {
  HistorySeries series;
  series.name = "ssn";

  std::stringstream ss(body);
  std::string line;
  std::vector<HistoryPoint> points;

  while (std::getline(ss, line)) {
    if (line.empty() || line[0] == '#' || line[0] == ':')
      continue;

    int y, m, d, ssn, flux;
    if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &ssn, &flux) ==
        5) {
      struct tm t = {0};
      t.tm_year = y - 1900;
      t.tm_mon = m - 1;
      t.tm_mday = d;
      points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                        Astronomy::portable_timegm(&t)),
                                    (float)ssn));
    }
  }

  if (points.size() > 30)
    points.erase(points.begin(), points.end() - 30);
  series.points = points;
  if (!points.empty()) {
    series.minValue = points[0].value;
    series.maxValue = points[0].value;
    for (const auto &p : points) {
      series.minValue = std::min(series.minValue, p.value);
      series.maxValue = std::max(series.maxValue, p.value);
    }
    series.valid = true;
  }
  store_->update("ssn", series);
}

void HistoryProvider::processKp(const std::string &body) {
  HistorySeries series;
  series.name = "kp";

  std::stringstream ss(body);
  std::string line;
  std::vector<HistoryPoint> points;

  while (std::getline(ss, line)) {
    if (line.empty() || line[0] == '#' || line[0] == ':')
      continue;

    // Format: YYYY MM DD  A-index  K-index
    int y, m, d, aIndex, kIndex;
    if (std::sscanf(line.c_str(), "%d %d %d %d %d", &y, &m, &d, &aIndex,
                    &kIndex) == 5) {
      struct tm t = {0};
      t.tm_year = y - 1900;
      t.tm_mon = m - 1;
      t.tm_mday = d;
      points.push_back(HistoryPoint(std::chrono::system_clock::from_time_t(
                                        Astronomy::portable_timegm(&t)),
                                    (float)kIndex));
    }
  }

  if (points.size() > 30)
    points.erase(points.begin(), points.end() - 30);
  series.points = points;
  if (!points.empty()) {
    series.valid = true;
    series.minValue = 0;
    series.maxValue = 9; // Kp is 0-9
  }
  store_->update("kp", series);
}

void HistoryProvider::fetchXray() {
  net_.fetchAsync(XRAY_URL, [this](std::string body) {
    if (!body.empty())
      processXray(body);
  });
}

void HistoryProvider::processXray(const std::string &body) {
  HistorySeries series;
  series.name = "xray";

  try {
    auto j = nlohmann::json::parse(body);
    if (!j.is_array())
      return;

    // Group by date (YYYY-MM-DD), keep peak flux per day in the 1-8A channel
    std::map<std::string, float> dailyPeak;

    for (const auto &entry : j) {
      if (entry.value("energy", "") != "1-8A")
        continue;
      std::string timeTag = entry.value("time_tag", "");
      if (timeTag.size() < 10)
        continue;
      std::string date = timeTag.substr(0, 10); // YYYY-MM-DD
      float flux = entry.value("flux", 0.0f);
      if (flux <= 0)
        continue;
      auto it = dailyPeak.find(date);
      if (it == dailyPeak.end() || flux > it->second)
        dailyPeak[date] = flux;
    }

    std::vector<HistoryPoint> points;
    for (auto &[dateStr, peak] : dailyPeak) {
      int y, m, d;
      if (std::sscanf(dateStr.c_str(), "%d-%d-%d", &y, &m, &d) != 3)
        continue;
      struct tm t = {0};
      t.tm_year = y - 1900;
      t.tm_mon = m - 1;
      t.tm_mday = d;
      // Store log10(flux): A-class ~-8, C-class ~-6, M-class ~-5, X-class ~-4
      float logFlux = std::log10(peak);
      points.push_back(HistoryPoint(
          std::chrono::system_clock::from_time_t(Astronomy::portable_timegm(&t)),
          logFlux));
    }

    if (points.size() > 30)
      points.erase(points.begin(), points.end() - 30);

    series.points = points;
    if (!points.empty()) {
      series.minValue = -9.0f; // Below A-class
      series.maxValue = -3.0f; // Above X10
      series.valid = true;
    }
  } catch (...) {
    return;
  }

  store_->update("xray", series);
}
