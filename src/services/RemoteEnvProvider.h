#pragma once

#include "../core/WeatherData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <string>

class RemoteEnvProvider {
public:
  RemoteEnvProvider(NetworkManager& net, std::shared_ptr<WeatherStore> store, const std::string& url);
  ~RemoteEnvProvider();

  void start();
  void stop();

  bool isAvailable() const { return available_; }

private:
  void worker();

  NetworkManager& net_;
  std::shared_ptr<WeatherStore> store_;
  std::string url_;
  std::atomic<bool> running_{false};
  std::atomic<bool> available_{false};
  std::mutex stopMutex_;
  std::condition_variable stopCv_;
  std::thread thread_;
};
