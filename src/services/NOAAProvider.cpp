#include "NOAAProvider.h"

#include "../core/Astronomy.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include <chrono>
#include <cstdio>
#include <nlohmann/json.hpp>

NOAAProvider::NOAAProvider(NetworkManager &net,
                           std::shared_ptr<SolarDataStore> store,
                           std::shared_ptr<AuroraHistoryStore> auroraStore,
                           HamClockState *state)
    : net_(net), store_(std::move(store)), auroraStore_(std::move(auroraStore)),
      state_(state) {}

void NOAAProvider::fetch() {
  LOG_I("NOAAProvider", "Starting solar data fetch cycle");
  fetchKIndex();
  fetchSFI();
  fetchSN();
  fetchPlasma();
  fetchMag();
  fetchDST();
  fetchAurora();
  fetchDRAP();
}

void NOAAProvider::fetchKIndex() {
  auto store = store_;
  auto state = state_;
  net_.fetchAsync(K_INDEX_URL, [store, state](std::string body) {
    if (body.empty()) {
      if (state) {
        auto &s = state->services["NOAA:KIndex"];
        s.ok = false;
        s.lastError = "Empty response";
      }
      return;
    }
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() < 2) {
      if (state)
        state->services["NOAA:KIndex"].ok = false;
      return;
    }

    const auto &row = j.back();
    if (!row.is_array() || row.size() < 3)
      return;

    try {
      auto data = store->get();
      double kp = std::stod(row[1].get<std::string>());
      data.k_index = static_cast<int>(kp);
      data.a_index = std::stoi(row[2].get<std::string>());
      data.last_updated = std::chrono::system_clock::now();
      data.valid = true;
      store->set(data);
      if (state) {
        auto &s = state->services["NOAA:KIndex"];
        s.ok = true;
        s.lastSuccess = std::chrono::system_clock::now();
      }
      LOG_I("NOAAProvider", "Updated K-Index: K={}", data.k_index);
    } catch (const std::exception &e) {
      LOG_E("NOAAProvider", "KP parse error: {}", e.what());
    }
  });
}

void NOAAProvider::fetchSFI() {
  auto store = store_;
  net_.fetchAsync(SFI_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array())
      return;

    try {
      auto data = store->get();
      // Try to find the flux value. Some variants of the JSON use "Flux" or
      // index 1.
      double flux = 0;
      if (j.is_object() && j.contains("Flux")) {
        flux = std::stod(j["Flux"].get<std::string>());
      } else if (j.is_array() && j.size() >= 2 && j.back().is_array()) {
        flux = std::stod(j.back()[1].get<std::string>());
      }
      if (flux > 0) {
        data.sfi = static_cast<int>(flux);
        data.valid = true;
        store->set(data);
        LOG_D("NOAAProvider", "SFI={}", data.sfi);
      }
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchSN() {
  auto store = store_;
  net_.fetchAsync(SN_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array())
      return;

    try {
      // Get current time string YYYY-MM
      auto now = std::chrono::system_clock::now();
      std::time_t t_c = std::chrono::system_clock::to_time_t(now);
      std::tm tm{};
      Astronomy::portable_localtime(&t_c, &tm);
      char buffer[8];
      std::strftime(buffer, sizeof(buffer), "%Y-%m", &tm);
      std::string current_month(buffer);

      double ssn = -1.0;

      // Find matching entry
      for (const auto &item : j) {
        if (item.contains("time-tag") &&
            item["time-tag"].get<std::string>() == current_month) {
          if (item.contains("predicted_ssn")) {
            ssn = item["predicted_ssn"].get<double>();
            break;
          }
        }
      }

      if (ssn >= 0) {
        auto data = store->get();
        data.sunspot_number = static_cast<int>(ssn);
        data.valid = true;
        store->set(data);
        LOG_D("NOAAProvider", "SN={} (Predicted for {})", data.sunspot_number,
              current_month);
      } else {
        LOG_W("NOAAProvider", "No SN prediction found for {}", current_month);
      }
    } catch (const std::exception &e) {
      LOG_E("NOAAProvider", "SN parse error: {}", e.what());
    }
  });
}

void NOAAProvider::fetchPlasma() {
  auto store = store_;
  net_.fetchAsync(PLASMA_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() < 2)
      return;

    try {
      auto data = store->get();
      const auto &row = j.back();
      // row: [time, density, speed, temp]
      if (row[1].is_string())
        data.solar_wind_density = std::stod(row[1].get<std::string>());
      if (row[2].is_string())
        data.solar_wind_speed = std::stod(row[2].get<std::string>());
      data.valid = true;
      store->set(data);
      LOG_D("NOAAProvider", "Wind={:.1f} km/s, Dense={:.1f}",
            data.solar_wind_speed, data.solar_wind_density);
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchMag() {
  auto store = store_;
  net_.fetchAsync(MAG_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() < 2)
      return;

    try {
      auto data = store->get();
      const auto &row = j.back();
      // row: [time, bx, by, bz, lon, lat, bt]
      if (row[3].is_string())
        data.bz =
            static_cast<int>(std::round(std::stod(row[3].get<std::string>())));
      if (row[6].is_string())
        data.bt =
            static_cast<int>(std::round(std::stod(row[6].get<std::string>())));
      data.valid = true;
      store->set(data);
      LOG_D("NOAAProvider", "Bz={}, Bt={}", data.bz, data.bt);
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchDST() {
  auto store = store_;
  net_.fetchAsync(DST_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() < 2)
      return;

    try {
      auto data = store->get();
      data.dst = std::stoi(j.back()[1].get<std::string>());
      data.valid = true;
      store->set(data);
      LOG_D("NOAAProvider", "DST={}", data.dst);
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchAurora() {
  auto store = store_;
  auto auroraStore = auroraStore_;
  net_.fetchAsync(AURORA_URL, [store, auroraStore](std::string body) {
    if (body.empty())
      return;

    try {
      float max_percent = 0;
      bool found_any = false;

      // Manual parse of JSON grid coordinates
      // Format: "coordinates":[[lon,lat,val],...]
      // spacewx.cpp: scan for [n,n,n]
      size_t coords_pos = body.find("\"coordinates\"");
      if (coords_pos != std::string::npos) {
        size_t p = coords_pos;
        while ((p = body.find('[', p)) != std::string::npos) {
          int lon, lat, val;
          if (sscanf(body.c_str() + p, "[%d,%d,%d]", &lon, &lat, &val) == 3) {
            // Note: sscanf format string must match spacing in JSON or be
            // tolerant.
            // JSON can be [0,-90,3] or [ 0, -90, 3 ].
            // spacewx.cpp uses strict "[%d, %d, %d]".
            // Let's broaden it slightly or assume standard JSON.
            // But let's verify if sscanf works with spaces (it skips leading
            // whitespace for %d).
            if (val > max_percent)
              max_percent = (float)val;
            found_any = true;
          } else if (sscanf(body.c_str() + p, "[%d, %d, %d]", &lon, &lat,
                            &val) == 3) {
            // Try with spaces
            if (val > max_percent)
              max_percent = (float)val;
            found_any = true;
          }
          p++;
        }
      }

      if (found_any) {
        auto data = store->get();
        data.aurora = static_cast<int>(max_percent);
        data.valid = true;
        store->set(data);

        // Also save to history store for graphing
        if (auroraStore) {
          // If this is the first point, add a duplicate 30 minutes ago
          // so the graph has something to show immediately
          if (!auroraStore->hasData()) {
            auroraStore->addPoint(max_percent);
            LOG_D("NOAAProvider", "Seeded Aurora history with initial point");
          }
          auroraStore->addPoint(max_percent);
        }

        LOG_D("NOAAProvider", "Aurora={} %", data.aurora);
      }
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchDRAP() {
  auto store = store_;
  net_.fetchAsync(DRAP_URL, [store](std::string body) {
    if (body.empty())
      return;

    try {
      float max_freq = 0;
      bool found_any = false;

      std::stringstream ss(body);
      std::string line;
      while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '\r')
          continue;

        size_t pipe_pos = line.find('|');
        if (pipe_pos != std::string::npos) {
          std::string freqs = line.substr(pipe_pos + 1);
          std::stringstream ss_vals(freqs);
          float val;
          while (ss_vals >> val) {
            if (val > max_freq)
              max_freq = val;
            found_any = true;
          }
        }
      }

      if (found_any) {
        auto data = store->get();
        // Storing float frequency into int field (rounding) per existing
        // pattern
        data.drap = static_cast<int>(std::round(max_freq));
        data.valid = true;
        store->set(data);
        LOG_D("NOAAProvider", "DRAP={:.1f} MHz (stored as {})", max_freq,
              data.drap);
      }
    } catch (...) {
    }
  });
}
