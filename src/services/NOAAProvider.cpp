#include "NOAAProvider.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>

NOAAProvider::NOAAProvider(NetworkManager &net,
                           std::shared_ptr<SolarDataStore> store)
    : net_(net), store_(std::move(store)) {}

void NOAAProvider::fetch() {
  fetchKIndex();
  fetchSFI();
  fetchSN();
  fetchPlasma();
  fetchMag();
  fetchDST();
  fetchAurora();
}

void NOAAProvider::fetchKIndex() {
  auto store = store_;
  net_.fetchAsync(K_INDEX_URL, [store](std::string body) {
    if (body.empty())
      return;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() < 2)
      return;

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
      std::fprintf(stderr, "NOAAProvider: K=%d A=%d\n", data.k_index,
                   data.a_index);
    } catch (...) {
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
        std::fprintf(stderr, "NOAAProvider: SFI=%d\n", data.sfi);
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
    if (j.is_discarded() || !j.is_array() || j.size() < 2)
      return;

    try {
      auto data = store->get();
      data.sunspot_number = std::stoi(j.back()[1].get<std::string>());
      data.valid = true;
      store->set(data);
      std::fprintf(stderr, "NOAAProvider: SN=%d\n", data.sunspot_number);
    } catch (...) {
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
      std::fprintf(stderr, "NOAAProvider: Wind=%.1f km/s, Dense=%.1f\n",
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
      std::fprintf(stderr, "NOAAProvider: Bz=%d, Bt=%d\n", data.bz, data.bt);
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
      std::fprintf(stderr, "NOAAProvider: DST=%d\n", data.dst);
    } catch (...) {
    }
  });
}

void NOAAProvider::fetchAurora() {
  auto store = store_;
  net_.fetchAsync(AURORA_URL, [store](std::string body) {
    if (body.empty())
      return;
    size_t lastNl = body.find_last_not_of("\r\n");
    if (lastNl == std::string::npos)
      return;
    size_t startOfLine = body.find_last_of("\r\n", lastNl);
    std::string line = body.substr(
        startOfLine == std::string::npos ? 0 : startOfLine + 1,
        lastNl - (startOfLine == std::string::npos ? -1 : startOfLine));

    try {
      std::vector<std::string> tokens;
      size_t pos = 0;
      while ((pos = line.find_first_not_of(" \t", pos)) != std::string::npos) {
        size_t end = line.find_first_of(" \t", pos);
        tokens.push_back(line.substr(pos, end - pos));
        pos = end;
      }

      if (tokens.size() >= 4) {
        auto data = store->get();
        int north = std::stoi(tokens[2]);
        int south = std::stoi(tokens[3]);
        data.aurora = std::max(north, south);
        data.valid = true;
        store->set(data);
        std::fprintf(stderr, "NOAAProvider: Aurora=%d GW\n", data.aurora);
      }
    } catch (...) {
    }
  });
}
