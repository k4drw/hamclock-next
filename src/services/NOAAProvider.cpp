#include "NOAAProvider.h"

#include "../core/Astronomy.h"
#include "../core/Constants.h"
#include "../core/HamClockState.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_map>

NOAAProvider::NOAAProvider(NetworkManager &net,
                           std::shared_ptr<SolarDataStore> store,
                           std::shared_ptr<AuroraHistoryStore> auroraStore,
                           std::shared_ptr<XRayHistoryStore> xrayStore,
                           HamClockState *state)
    : net_(net), store_(std::move(store)), auroraStore_(std::move(auroraStore)),
      xrayStore_(std::move(xrayStore)), state_(state) {}

// Calculate R-scale (Radio Blackouts) from X-ray flux
// R1: >= 1e-5, R2: >= 5e-5, R3: >= 1e-4, R4: >= 1e-3, R5: >= 2e-3
static int calculateRScale(double xray_flux) {
  if (xray_flux >= 2e-3)
    return 5; // R5 (Extreme)
  if (xray_flux >= 1e-3)
    return 4; // R4 (Severe)
  if (xray_flux >= 1e-4)
    return 3; // R3 (Strong)
  if (xray_flux >= 5e-5)
    return 2; // R2 (Moderate)
  if (xray_flux >= 1e-5)
    return 1; // R1 (Minor)
  return 0;   // R0 (No event)
}

// Calculate S-scale (Solar Radiation Storms) from >=10 MeV proton flux
// S1: >= 10, S2: >= 100, S3: >= 1000, S4: >= 10000, S5: >= 100000
static int calculateSScale(double proton_flux) {
  if (proton_flux >= 1e5)
    return 5; // S5 (Extreme)
  if (proton_flux >= 1e4)
    return 4; // S4 (Severe)
  if (proton_flux >= 1e3)
    return 3; // S3 (Strong)
  if (proton_flux >= 100)
    return 2; // S2 (Moderate)
  if (proton_flux >= 10)
    return 1; // S1 (Minor)
  return 0;   // S0 (No event)
}

// Calculate G-scale (Geomagnetic Storms) from Kp index
// G1: Kp=5, G2: Kp=6, G3: Kp=7, G4: Kp=8, G5: Kp=9
static int calculateGScale(int kp_index) {
  if (kp_index >= 9)
    return 5; // G5 (Extreme)
  if (kp_index >= 8)
    return 4; // G4 (Severe)
  if (kp_index >= 7)
    return 3; // G3 (Strong)
  if (kp_index >= 6)
    return 2; // G2 (Moderate)
  if (kp_index >= 5)
    return 1; // G1 (Minor)
  return 0;   // G0 (No event)
}

void NOAAProvider::fetch() {
  LOG_I("NOAAProvider", "Starting solar data fetch cycle");
  fetchKIndex();
  fetchSFI();
  fetchSN();
  fetchPlasma();
  fetchMag();
  fetchDST();
  fetchAurora();
  if (auroraStore_ && auroraStore_->needsBackfill())
    fetchAuroraHistory();
  fetchDRAP();
  fetchXRay();
  fetchProtonFlux();
}

void NOAAProvider::fetchKIndex() {
  auto state = state_;
  net_.fetchAsync(K_INDEX_URL, [state](std::string body) {
    if (body.empty()) {
      if (state) {
        auto &s = state->services["NOAA:KIndex"];
        s.ok = false;
        s.lastError = "Empty response";
      }
      return;
    }

    WorkerService::getInstance().submitTask([body, state]() {
      auto j = nlohmann::json::parse(body, nullptr, false);
      if (j.is_discarded() || !j.is_array() || j.size() < 2) {
        // We can't easily update state->services from here if it's not
        // thread-safe. But HamClockState has a mutex for its services map.
        return;
      }

      const auto &row = j.back();
      if (!row.is_array() || row.size() < 3)
        return;

      auto *update = new SolarData();
      double kp = StringUtils::safe_stod(row[1].get<std::string>());
      update->k_index = static_cast<float>(kp);
      update->a_index = StringUtils::safe_stoi(row[2].get<std::string>());
      update->noaa_g_scale = calculateGScale(update->k_index);
      update->last_updated = std::chrono::system_clock::now();
      update->valid = true;

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
      event.user.code = static_cast<int>(UpdateType::KIndex);
      event.user.data1 = update;
      SDL_PushEvent(&event);

      if (state) {
        auto &s = state->services["NOAA:KIndex"];
        s.ok = true;
        s.lastSuccess = std::chrono::system_clock::now();
      }
      LOG_I("NOAAProvider", "Offloaded K-Index update: K={}, G-scale=G{}",
            update->k_index, update->noaa_g_scale);
    });
  });
}

void NOAAProvider::fetchSFI() {
  net_.fetchAsync(SFI_URL, [](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body]() {
      auto j = nlohmann::json::parse(body, nullptr, false);
      if (j.is_discarded() || !j.is_array())
        return;

      double flux = 0;
      if (j.is_object() && j.contains("Flux")) {
        flux = StringUtils::safe_stod(j["Flux"].get<std::string>());
      } else if (j.is_array() && j.size() >= 2 && j.back().is_array()) {
        flux = StringUtils::safe_stod(j.back()[1].get<std::string>());
      }

      if (flux > 0) {
        auto *update = new SolarData();
        update->sfi = static_cast<int>(flux);
        update->valid = true;

        SDL_Event event;
        SDL_zero(event);
        event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
        event.user.code = static_cast<int>(UpdateType::SFI);
        event.user.data1 = update;
        SDL_PushEvent(&event);
      }
    });
  });
}

void NOAAProvider::fetchSN() {
  net_.fetchAsync(SN_URL, [](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body]() {
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
          auto *update = new SolarData();
          update->sunspot_number = static_cast<int>(ssn);
          update->valid = true;

          SDL_Event event;
          SDL_zero(event);
          event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
          event.user.code = static_cast<int>(UpdateType::SN);
          event.user.data1 = update;
          SDL_PushEvent(&event);
          LOG_D("NOAAProvider", "Offloaded SN update={} (Predicted for {})",
                update->sunspot_number, current_month);
        } else {
          LOG_W("NOAAProvider", "No SN prediction found for {}", current_month);
        }
      } catch (const std::exception &e) {
        LOG_E("NOAAProvider", "SN parse error: {}", e.what());
      }
    });
  });
}

void NOAAProvider::fetchPlasma() {
  net_.fetchAsync(PLASMA_URL, [](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body]() {
      auto j = nlohmann::json::parse(body, nullptr, false);
      if (j.is_discarded() || !j.is_array() || j.size() < 2)
        return;

      const auto &row = j.back();
      auto *update = new SolarData();
      // row: [time, density, speed, temp]
      if (row[1].is_string())
        update->solar_wind_density =
            StringUtils::safe_stod(row[1].get<std::string>());
      if (row[2].is_string())
        update->solar_wind_speed =
            StringUtils::safe_stod(row[2].get<std::string>());
      update->valid = true;

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
      event.user.code = static_cast<int>(UpdateType::Plasma);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}

void NOAAProvider::fetchMag() {
  net_.fetchAsync(MAG_URL, [](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body]() {
      auto j = nlohmann::json::parse(body, nullptr, false);
      if (j.is_discarded() || !j.is_array() || j.size() < 2)
        return;

      const auto &row = j.back();
      auto *update = new SolarData();
      // row: [time, bx, by, bz, lon, lat, bt]
      if (row[3].is_string())
        update->bz = static_cast<int>(
            std::round(StringUtils::safe_stod(row[3].get<std::string>())));
      if (row[6].is_string())
        update->bt = static_cast<int>(
            std::round(StringUtils::safe_stod(row[6].get<std::string>())));
      update->valid = true;

      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
      event.user.code = static_cast<int>(UpdateType::Mag);
      event.user.data1 = update;
      SDL_PushEvent(&event);
    });
  });
}

void NOAAProvider::fetchDST() {
  net_.fetchAsync(DST_URL, [](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body]() {
      auto j = nlohmann::json::parse(body, nullptr, false);
      if (j.is_discarded() || !j.is_array() || j.size() < 2)
        return;

      try {
        auto *update = new SolarData();
        update->dst = StringUtils::safe_stoi(j.back()[1].get<std::string>());
        update->valid = true;

        SDL_Event event;
        SDL_zero(event);
        event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
        event.user.code = static_cast<int>(UpdateType::DST);
        event.user.data1 = update;
        SDL_PushEvent(&event);
      } catch (...) {
      }
    });
  });
}

void NOAAProvider::fetchAurora() {
  auto auroraStore = auroraStore_;
  net_.fetchAsync(AURORA_URL, [auroraStore](std::string body) {
    if (body.empty())
      return;

    WorkerService::getInstance().submitTask([body, auroraStore]() {
      try {
        float max_percent = 0;
        bool found_any = false;

        // Manual parse of JSON grid coordinates
        // Format: "coordinates":[[lon,lat,val],...]
        size_t coords_pos = body.find("\"coordinates\"");
        if (coords_pos != std::string::npos) {
          size_t p = coords_pos;
          while ((p = body.find('[', p)) != std::string::npos) {
            int lon, lat, val;
            if (sscanf(body.c_str() + p, "[%d,%d,%d]", &lon, &lat, &val) == 3) {
              if (val > max_percent)
                max_percent = (float)val;
              found_any = true;
            } else if (sscanf(body.c_str() + p, "[%d, %d, %d]", &lon, &lat,
                              &val) == 3) {
              if (val > max_percent)
                max_percent = (float)val;
              found_any = true;
            }
            p++;
          }
        }

        if (found_any) {
          // Push to SolarDataStore
          auto *solarUpdate = new SolarData();
          solarUpdate->aurora = static_cast<int>(max_percent);
          solarUpdate->valid = true;

          SDL_Event event;
          SDL_zero(event);
          event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
          event.user.code = static_cast<int>(UpdateType::Aurora);
          event.user.data1 = solarUpdate;
          SDL_PushEvent(&event);

          // Push to AuroraHistoryStore
          if (auroraStore) {
            LOG_I("NOAAProvider", "Aurora updated: {}%%", max_percent);
            // If this is the first point, add a duplicate
            if (!auroraStore->hasData()) {
              auto *percent1 = new float(max_percent);
              SDL_Event event1;
              SDL_zero(event1);
              event1.type =
                  HamClock::AE_BASE_EVENT + HamClock::AE_AURORA_DATA_READY;
              event1.user.data1 = percent1;
              SDL_PushEvent(&event1);
            }

            auto *percent2 = new float(max_percent);
            SDL_Event event2;
            SDL_zero(event2);
            event2.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_AURORA_DATA_READY;
            event2.user.data1 = percent2;
            SDL_PushEvent(&event2);
          }
        }
      } catch (...) {
      }
    });
  });
}

// Shared state for the two parallel solar-wind backfill fetches.
// Lives until both callbacks have fired (held by shared_ptr in each lambda).
struct SolarWindBackfill {
  std::mutex mu;
  int pending{2};
  // keyed by unix timestamp rounded to the nearest minute
  std::unordered_map<int64_t, std::pair<float, float>> mag; // t → (bz, bt_transverse)
  std::unordered_map<int64_t, float> speed;                 // t → km/s
  std::shared_ptr<AuroraHistoryStore> store;
};

// Parse NOAA "YYYY-MM-DD HH:MM:SS.sss" time-tag → unix seconds, rounded to
// the nearest minute.  Returns -1 on failure.
static int64_t parseNoaaMinute(const std::string &ts) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (sscanf(ts.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6)
    return -1;
  std::tm tm{};
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_sec = s;
  tm.tm_isdst = -1;
  std::time_t t = Astronomy::portable_timegm(&tm);
  if (t < 0)
    return -1;
  return (static_cast<int64_t>(t) / 60) * 60; // round to minute
}

static void computeAndMerge(std::shared_ptr<SolarWindBackfill> state) {
  std::vector<AuroraDataPoint> points;
  points.reserve(state->speed.size());

  for (const auto &[t, v] : state->speed) {
    auto it = state->mag.find(t);
    if (it == state->mag.end())
      continue;

    float bz = it->second.first;
    float bt = it->second.second; // sqrt(By² + Bz²)

    if (bt < 0.5f || v < 50.0f)
      continue; // skip bad/null data

    // Newell coupling: dΦ/dt = V^(4/3) × Bt^(2/3) × sin^(8/3)(θ/2)
    // sin²(θ/2) = (1 - Bz/Bt) / 2  (clock angle; 1 when Bz fully southward)
    float sin2 = std::max(0.0f, (1.0f - bz / bt) * 0.5f);
    float coupling = std::pow(v, 4.0f / 3.0f) * std::pow(bt, 2.0f / 3.0f) *
                     std::pow(sin2, 4.0f / 3.0f);

    // Scale to 0-100%: power-law, ~50 000 Wb/s ≈ extreme storm
    float pct =
        std::min(100.0f, std::pow(coupling / 50000.0f, 0.4f) * 100.0f);

    AuroraDataPoint dp;
    dp.percent = pct;
    dp.timestamp =
        std::chrono::system_clock::from_time_t(static_cast<std::time_t>(t));
    points.push_back(dp);
  }

  if (!points.empty()) {
    state->store->mergePoints(std::move(points));
    LOG_I("NOAAProvider", "Aurora backfill: {} solar-wind points merged",
          points.size());
  }
}

void NOAAProvider::fetchAuroraHistory() {
  auto backfill = std::make_shared<SolarWindBackfill>();
  backfill->store = auroraStore_;

  // --- Fetch 1: solar-wind speed (plasma-7-day) ---
  net_.fetchAsync(PLASMA_7D_URL, [backfill](std::string body) {
    WorkerService::getInstance().submitTask([body, backfill]() {
      try {
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (!j.is_discarded() && j.is_array()) {
          for (const auto &row : j) {
            if (!row.is_array() || row.size() < 3)
              continue;
            if (!row[0].is_string() || row[2].is_null())
              continue;
            int64_t t = parseNoaaMinute(row[0].get<std::string>());
            if (t < 0)
              continue;
            float v = static_cast<float>(
                StringUtils::safe_stod(row[2].get<std::string>()));
            if (v > 0)
              backfill->speed[t] = v;
          }
        }
      } catch (...) {
      }
      std::lock_guard<std::mutex> lk(backfill->mu);
      if (--backfill->pending == 0)
        computeAndMerge(backfill);
    });
  });

  // --- Fetch 2: IMF Bz + By (mag-7-day) ---
  net_.fetchAsync(MAG_7D_URL, [backfill](std::string body) {
    WorkerService::getInstance().submitTask([body, backfill]() {
      try {
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (!j.is_discarded() && j.is_array()) {
          for (const auto &row : j) {
            // row: [time, bx, by, bz, lon, lat, bt]
            if (!row.is_array() || row.size() < 7)
              continue;
            if (!row[0].is_string() || row[2].is_null() || row[3].is_null())
              continue;
            int64_t t = parseNoaaMinute(row[0].get<std::string>());
            if (t < 0)
              continue;
            float by = static_cast<float>(
                StringUtils::safe_stod(row[2].get<std::string>()));
            float bz = static_cast<float>(
                StringUtils::safe_stod(row[3].get<std::string>()));
            float bt = std::sqrt(by * by + bz * bz); // transverse field
            backfill->mag[t] = {bz, bt};
          }
        }
      } catch (...) {
      }
      std::lock_guard<std::mutex> lk(backfill->mu);
      if (--backfill->pending == 0)
        computeAndMerge(backfill);
    });
  });
}

void NOAAProvider::fetchDRAP() {
  LOG_I("NOAAProvider", "DRAP fetch started (drapStore_={})", drapStore_ ? "set" : "null");
  auto drapStore = drapStore_;
  net_.fetchAsync(DRAP_URL, [drapStore](std::string body) {
    if (body.empty()) {
      LOG_W("NOAAProvider", "DRAP fetch returned empty body");
      return;
    }

    WorkerService::getInstance().submitTask([body, drapStore]() {
      try {
        float max_freq = 0;
        bool found_any = false;
        DRAPGrid grid;
        int maxCols = 0;

        std::stringstream ss(body);
        std::string line;
        while (std::getline(ss, line)) {
          if (line.empty() || line[0] == '#' || line[0] == '\r')
            continue;

          size_t pipe_pos = line.find('|');
          if (pipe_pos == std::string::npos)
            continue;

          std::string freqs = line.substr(pipe_pos + 1);
          std::stringstream ss_vals(freqs);
          float val;
          int rowCols = 0;
          while (ss_vals >> val) {
            grid.cells.push_back(val);
            if (val > max_freq)
              max_freq = val;
            found_any = true;
            rowCols++;
          }
          if (rowCols > 0) {
            grid.rows++;
            if (rowCols > maxCols)
              maxCols = rowCols;
          }
        }

        if (found_any) {
          // Populate grid store for DRAP map overlay
          if (drapStore) {
            grid.cols = maxCols;
            grid.valid = true;
            grid.updated = std::chrono::system_clock::now();
            drapStore->set(grid);
            LOG_I("NOAAProvider", "DRAP grid stored: {}x{}, {} cells, max={:.1f} MHz",
                  grid.rows, grid.cols, (int)grid.cells.size(), max_freq);
          } else {
            LOG_W("NOAAProvider", "DRAP grid parsed but drapStore is null (rows={} cols={})",
                  grid.rows, maxCols);
          }

          auto *update = new SolarData();
          update->drap = static_cast<int>(std::round(max_freq));
          update->valid = true;

          SDL_Event event;
          SDL_zero(event);
          event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
          event.user.code = static_cast<int>(UpdateType::DRAP);
          event.user.data1 = update;
          SDL_PushEvent(&event);
        }
      } catch (...) {
      }
    });
  });
}

void NOAAProvider::fetchXRay() {
  auto state = state_;
  auto xrayStore = xrayStore_;
  net_.fetchAsync(XRAY_URL, [state, xrayStore](std::string body) {
    if (body.empty()) {
      if (state) {
        auto &s = state->services["NOAA:XRay"];
        s.ok = false;
        s.lastError = "Empty response";
      }
      return;
    }

    WorkerService::getInstance().submitTask([body, state, xrayStore]() {
      try {
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (j.is_discarded() || !j.is_array() || j.empty()) {
          if (state) {
            state->services["NOAA:XRay"].ok = false;
            state->services["NOAA:XRay"].lastError = "Invalid JSON";
          }
          return;
        }

        // Collect all 0.1-0.8nm entries; also find the latest for SolarData
        double latest_flux = 0.0;
        bool found = false;

        for (const auto &entry : j) {
          if (!entry.contains("energy") || !entry.contains("flux"))
            continue;
          if (entry["energy"].get<std::string>() != "0.1-0.8nm")
            continue;

          double flux = entry["flux"].get<double>();

          // Push to history store with parsed timestamp
          if (xrayStore && entry.contains("time_tag") &&
              entry["time_tag"].is_string()) {
            int64_t t =
                parseNoaaMinute(entry["time_tag"].get<std::string>());
            if (t >= 0) {
              XRayDataPoint pt;
              pt.timestamp = std::chrono::system_clock::from_time_t(
                  static_cast<std::time_t>(t));
              pt.flux = flux;
              xrayStore->push(pt);
            }
          }

          latest_flux = flux; // last entry in array is the most recent
          found = true;
        }

        if (found) {
          auto *update = new SolarData();
          update->xray_flux = latest_flux;
          update->noaa_r_scale = calculateRScale(latest_flux);
          update->valid = true;

          SDL_Event event;
          SDL_zero(event);
          event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
          event.user.code = static_cast<int>(UpdateType::XRay);
          event.user.data1 = update;
          SDL_PushEvent(&event);

          if (state) {
            auto &s = state->services["NOAA:XRay"];
            s.ok = true;
            s.lastSuccess = std::chrono::system_clock::now();
          }
        } else {
          if (state) {
            state->services["NOAA:XRay"].ok = false;
            state->services["NOAA:XRay"].lastError = "No 0.1-0.8nm data found";
          }
        }
      } catch (const std::exception &e) {
        LOG_E("NOAAProvider", "X-ray parse error: {}", e.what());
        if (state) {
          auto &s = state->services["NOAA:XRay"];
          s.ok = false;
          s.lastError = e.what();
        }
      }
    });
  });
}

void NOAAProvider::fetchProtonFlux() {
  auto state = state_;
  net_.fetchAsync(PROTON_URL, [state](std::string body) {
    if (body.empty()) {
      if (state) {
        auto &s = state->services["NOAA:ProtonFlux"];
        s.ok = false;
        s.lastError = "Empty response";
      }
      return;
    }

    WorkerService::getInstance().submitTask([body, state]() {
      try {
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (j.is_discarded() || !j.is_array() || j.empty()) {
          if (state) {
            state->services["NOAA:ProtonFlux"].ok = false;
            state->services["NOAA:ProtonFlux"].lastError = "Invalid JSON";
          }
          return;
        }

        // Find the most recent >=10 MeV proton flux entry
        double latest_flux = 0.0;
        bool found = false;

        for (auto it = j.rbegin(); it != j.rend(); ++it) {
          const auto &entry = *it;
          if (entry.contains("energy") && entry.contains("flux")) {
            std::string energy = entry["energy"].get<std::string>();
            if (energy == ">=10 MeV") {
              latest_flux = entry["flux"].get<double>();
              found = true;
              break;
            }
          }
        }

        if (found) {
          auto *update = new SolarData();
          update->proton_flux = latest_flux;
          update->noaa_s_scale = calculateSScale(latest_flux);
          update->valid = true;

          SDL_Event event;
          SDL_zero(event);
          event.type = HamClock::AE_BASE_EVENT + HamClock::AE_SOLAR_DATA_READY;
          event.user.code = static_cast<int>(UpdateType::ProtonFlux);
          event.user.data1 = update;
          SDL_PushEvent(&event);

          if (state) {
            auto &s = state->services["NOAA:ProtonFlux"];
            s.ok = true;
            s.lastSuccess = std::chrono::system_clock::now();
          }
        } else {
          if (state) {
            state->services["NOAA:ProtonFlux"].ok = false;
            state->services["NOAA:ProtonFlux"].lastError =
                "No >=10 MeV data found";
          }
        }
      } catch (const std::exception &e) {
        LOG_E("NOAAProvider", "Proton flux parse error: {}", e.what());
        if (state) {
          auto &s = state->services["NOAA:ProtonFlux"];
          s.ok = false;
          s.lastError = e.what();
        }
      }
    });
  });
}
