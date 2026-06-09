#pragma once

#include "../core/WebSocketTransport.h"
#include <mqtt.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

struct AppContext;

class MQTTService {
public:
  static MQTTService &getInstance() {
    static MQTTService instance;
    return instance;
  }

  void start();
  void stop();

  void publish(const std::string &topic, const std::string &payload, int qos = 0, bool retain = false);
  
  void publishState(struct AppContext& ctx);

private:
  MQTTService();
  ~MQTTService();

  void threadLoop();
  bool connect();
  void sendAutoDiscovery();

  WebSocketTransport transport_;
  struct mqtt_client mqttClient_;
  std::vector<uint8_t> txBuffer_;
  std::vector<uint8_t> rxBuffer_;

  std::thread workerThread_;
  std::atomic<bool> running_;
  std::recursive_mutex mutex_;

  uint32_t lastReconnectAttempt_ = 0;
  uint32_t lastPublishTime_ = 0;
  bool needAutoDiscovery_ = false;

  // Custom pal callbacks
  static ssize_t pal_send(void* context, const void* buf, size_t len);
  static ssize_t pal_recv(void* context, void* buf, size_t len);
};
