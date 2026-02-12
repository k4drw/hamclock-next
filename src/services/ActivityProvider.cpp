#include "ActivityProvider.h"
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ActivityProvider::ActivityProvider(NetworkManager &net,
                                   std::shared_ptr<ActivityDataStore> store)
    : net_(net), store_(store) {}

void ActivityProvider::fetch() {
  fetchDXPeds();
  fetchPOTA();
  fetchSOTA();
}

void ActivityProvider::fetchDXPeds() {
  net_.fetchAsync(DX_PEDS_URL, [this](std::string data) {
    if (data.empty()) {
      std::cerr << "Failed to fetch DXPeditions from NG3K" << std::endl;
      return;
    }

    ActivityData current = store_->get();
    current.dxpeds.clear();

    // Lightweight HTML scraping
    size_t pos = 0;
    auto now = std::chrono::system_clock::now();

    auto crackMonth = [](const std::string &m) -> int {
      static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      for (int i = 0; i < 12; i++) {
        if (m.find(months[i]) != std::string::npos)
          return i + 1;
      }
      return 0;
    };

    while ((pos = data.find("class=\"adxoitem\"", pos)) != std::string::npos) {
      auto findTagContent = [&](const std::string &html,
                                const std::string &className,
                                size_t &searchPos) -> std::string {
        std::string target = "class=\"" + className + "\"";
        size_t p = html.find(target, searchPos);
        if (p == std::string::npos)
          return "";
        size_t start = html.find(">", p);
        if (start == std::string::npos)
          return "";
        start++;
        size_t end = html.find("<", start);
        if (end == std::string::npos)
          return "";
        searchPos = end;
        return html.substr(start, end - start);
      };

      size_t rowPos = pos;
      std::string d1 = findTagContent(data, "date", rowPos);
      std::string d2 = findTagContent(data, "date", rowPos);
      std::string loc = findTagContent(data, "cty", rowPos);
      std::string call = findTagContent(data, "call", rowPos);

      if (call.find("<a") != std::string::npos) {
        size_t a_end = call.find(">");
        if (a_end != std::string::npos) {
          size_t a_close = call.find("</a", a_end);
          if (a_close != std::string::npos) {
            call = call.substr(a_end + 1, a_close - (a_end + 1));
          }
        }
      }

      if (!call.empty() && !d1.empty()) {
        DXPedition de;
        de.call = call;
        de.location = loc;

        int y1, dy1, y2, dy2;
        char m1[10], m2[10];
        if (sscanf(d1.c_str(), "%d %s %d", &y1, m1, &dy1) == 3 &&
            sscanf(d2.c_str(), "%d %s %d", &y2, m2, &dy2) == 3) {

          std::tm tm1 = {};
          tm1.tm_year = y1 - 1900;
          tm1.tm_mon = crackMonth(m1) - 1;
          tm1.tm_mday = dy1;
          de.startTime =
              std::chrono::system_clock::from_time_t(std::mktime(&tm1));

          std::tm tm2 = {};
          tm2.tm_year = y2 - 1900;
          tm2.tm_mon = crackMonth(m2) - 1;
          tm2.tm_mday = dy2;
          tm2.tm_hour = 23;
          tm2.tm_min = 59;
          de.endTime =
              std::chrono::system_clock::from_time_t(std::mktime(&tm2));

          auto yesterday = now - std::chrono::hours(24);
          if (de.endTime > yesterday) {
            current.dxpeds.push_back(de);
          }
        }
      }
      pos += 16;
    }

    current.lastUpdated = now;
    current.valid = true;
    store_->set(current);
  });
}

void ActivityProvider::fetchPOTA() {
  net_.fetchAsync(POTA_API_URL, [this](std::string data) {
    if (data.empty())
      return;
    try {
      auto j = json::parse(data);
      if (!j.is_array())
        return;

      ActivityData current = store_->get();
      // Only clear ONTA spots if this is the first one or we want to merge them
      // carefully
      // For now, let's keep it simple and just clear and re-add from both SOTA
      // and POTA
      // Actually, we should probably have separate lists or clear only when
      // starting a full cycle

      // Let's filter existing spots to remove POTA ones and re-add fresh
      auto it =
          std::remove_if(current.ontaSpots.begin(), current.ontaSpots.end(),
                         [](const ONTASpot &s) { return s.program == "POTA"; });
      current.ontaSpots.erase(it, current.ontaSpots.end());

      for (const auto &spot : j) {
        ONTASpot os;
        os.program = "POTA";
        os.call = spot.value("activator", "");
        os.ref = spot.value("reference", "");
        os.mode = spot.value("mode", "");
        try {
          std::string freq = spot.value("frequency", "0");
          os.freqKhz = std::stod(freq);
        } catch (...) {
          os.freqKhz = 0;
        }
        os.spottedAt = std::chrono::system_clock::now();

        if (!os.call.empty()) {
          current.ontaSpots.push_back(os);
        }
      }
      current.lastUpdated = std::chrono::system_clock::now();
      store_->set(current);
    } catch (...) {
    }
  });
}

void ActivityProvider::fetchSOTA() {
  net_.fetchAsync(SOTA_API_URL, [this](std::string data) {
    if (data.empty())
      return;
    try {
      auto j = json::parse(data);
      if (!j.is_array())
        return;

      ActivityData current = store_->get();
      auto it =
          std::remove_if(current.ontaSpots.begin(), current.ontaSpots.end(),
                         [](const ONTASpot &s) { return s.program == "SOTA"; });
      current.ontaSpots.erase(it, current.ontaSpots.end());

      for (const auto &spot : j) {
        ONTASpot os;
        os.program = "SOTA";
        os.call = spot.value("activatorCallsign", "");
        os.ref = spot.value("associationCode", "") + "/" +
                 spot.value("summitCode", "");
        os.mode = spot.value("mode", "");
        try {
          std::string freq = spot.value("frequency", "0");
          os.freqKhz = std::stod(freq) * 1000.0; // SOTA MHz to kHz? Check API
        } catch (...) {
          os.freqKhz = 0;
        }
        os.spottedAt = std::chrono::system_clock::now();

        if (!os.call.empty()) {
          current.ontaSpots.push_back(os);
        }
      }
      current.lastUpdated = std::chrono::system_clock::now();
      store_->set(current);
    } catch (...) {
    }
  });
}
