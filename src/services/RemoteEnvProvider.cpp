#include "RemoteEnvProvider.h"
#include "../core/Logger.h"
#include "../network/NetworkManager.h"
#include <nlohmann/json.hpp>
#include <cmath>

RemoteEnvProvider::RemoteEnvProvider(NetworkManager& net, std::shared_ptr<WeatherStore> store, const std::string& url)
    : net_(net), store_(store), url_(url) {}

RemoteEnvProvider::~RemoteEnvProvider() { stop(); }

void RemoteEnvProvider::start() {
  if (running_.exchange(true))
    return;

  if (url_.empty()) {
    LOG_W("EnvNet", "No network URL configured for environment sensor.");
    return;
  }

  thread_ = std::thread(&RemoteEnvProvider::worker, this);
}

void RemoteEnvProvider::stop() {
  if (running_.exchange(false)) {
    stopCv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void RemoteEnvProvider::worker() {
  LOG_I("EnvNet", "Starting network environment sensor polling at {}", url_);

  while (running_.load()) {
    net_.fetchAsync(url_, [this](const std::string& response) {
      if (response.empty()) {
        LOG_D("EnvNet", "Failed to fetch from {}", url_);
        return;
      }
      try {
        auto j = nlohmann::json::parse(response);
        if (j.contains("tempC") && j.contains("pressHpa") && j.contains("humidity")) {
          float tempC = j["tempC"].get<float>();
          float pressHpa = j["pressHpa"].get<float>();
          float humidity = j["humidity"].get<float>();

          WeatherData wd;
          wd.valid = true;
          wd.temp = tempC;
          wd.pressure = pressHpa;
          wd.humidity = static_cast<int>(std::round(humidity));
          wd.description = "Network Env";
          wd.lastUpdate = std::chrono::system_clock::now();
          if (j.contains("gasKOhms")) {
            wd.hasGas = true;
            wd.gasResistance = j["gasKOhms"].get<float>();
            if (j.contains("iaq")) {
              wd.iaq = j["iaq"].get<float>();
            } else {
              // Extremely rough faux-IAQ based on resistance if exact algorithm is omitted
              wd.iaq = std::max(0.0f, 500.0f - (wd.gasResistance / 50.0f)); 
            }
          }

          store_->update(wd);

          if (!available_.load()) {
            available_.store(true);
            LOG_I("EnvNet", "Network environment sensor is now online.");
          }

          if (wd.hasGas) {
            LOG_D("EnvNet", "T={:.1f}C P={:.1f}hPa H={}% IAQ={:.0f}", tempC, pressHpa, wd.humidity, wd.iaq);
          } else {
            LOG_D("EnvNet", "T={:.1f}C P={:.1f}hPa H={}%", tempC, pressHpa, wd.humidity);
          }
        } else {
          LOG_W("EnvNet", "Malformed JSON from network sensor: {}", response);
        }
      } catch (const nlohmann::json::parse_error& e) {
        LOG_W("EnvNet", "Failed to parse network sensor JSON: {}", e.what());
      }
    }, 0, true);

    // Wait 10 seconds before next poll
    std::unique_lock<std::mutex> lk(stopMutex_);
    stopCv_.wait_for(lk, std::chrono::seconds(10), [this] { return !running_.load(); });
  }
}
