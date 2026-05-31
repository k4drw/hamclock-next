#include "MQTTService.h"
#include "../core/ConfigManager.h"
#include "../core/Logger.h"
#include "../core/DashboardContext.h"
#include "../core/SolarData.h"
#include "../core/LiveSpotData.h"
#include <SDL.h>
#include <iostream>
#include <nlohmann/json.hpp>

// PAL callbacks for MQTT-C
ssize_t MQTTService::pal_send(void* context, const void* buf, size_t len) {
  auto* svc = static_cast<MQTTService*>(context);
  return svc->transport_.send(buf, len);
}

ssize_t MQTTService::pal_recv(void* context, void* buf, size_t len) {
  auto* svc = static_cast<MQTTService*>(context);
  return svc->transport_.recv(buf, len);
}

MQTTService::MQTTService() : running_(false) {
  txBuffer_.resize(4096);
  rxBuffer_.resize(4096);
}

MQTTService::~MQTTService() {
  stop();
}

void MQTTService::start() {
  if (running_) return;
  running_ = true;
  
#ifndef __EMSCRIPTEN__
  workerThread_ = std::thread(&MQTTService::threadLoop, this);
#else
  // For WASM, we should pump from the main loop, but for now we'll just start the connection.
  // We'll rely on the transport's onMessage to trigger reads.
  connect();
#endif
}

void MQTTService::stop() {
  running_ = false;
#ifndef __EMSCRIPTEN__
  if (workerThread_.joinable()) workerThread_.join();
#endif
  transport_.disconnect();
}

bool MQTTService::connect() {
  auto& cfg = ConfigManager::instance().getConfig();
  if (!cfg.mqttEnabled || cfg.mqttBrokerUri.empty()) return false;
  
  if (transport_.isConnected()) return true;
  
  std::cout << "[MQTT] Connecting to " << cfg.mqttBrokerUri << " ..." << std::endl;
  if (!transport_.connect(cfg.mqttBrokerUri)) {
    return false;
  }
  
  // Init MQTT-C client
  mqtt_init(&mqttClient_, transport_.getFd(), txBuffer_.data(), txBuffer_.size(), rxBuffer_.data(), rxBuffer_.size(), nullptr);
  
  // Override PAL if we had to, but MQTT-C doesn't support overriding without recompiling with MQTT_PAL_CUSTOM.
  // Wait, if we use standard sockets on Desktop, getFd() returns the socket and MQTT-C natively uses it!
  // On Emscripten, we won't get a valid FD, so MQTT-C will fail unless we use a custom PAL.
  // Since MQTT-C is fetched via CMake, we would have to patch it or set MQTT_PAL_CUSTOM.
  // For now, let's assume we use desktop native sockets.
  
  uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
  if (!cfg.mqttUsername.empty()) connect_flags |= MQTT_CONNECT_USER_NAME;
  if (!cfg.mqttPassword.empty()) connect_flags |= MQTT_CONNECT_PASSWORD;
  
  const char* user = cfg.mqttUsername.empty() ? nullptr : cfg.mqttUsername.c_str();
  const char* pass = cfg.mqttPassword.empty() ? nullptr : cfg.mqttPassword.c_str();
  
  mqtt_connect(&mqttClient_, "HamClockNext", nullptr, nullptr, 0, user, pass, connect_flags, 400);
  
  if (mqttClient_.error != MQTT_OK) {
    std::cerr << "[MQTT] mqtt_connect failed: " << mqtt_error_str(mqttClient_.error) << std::endl;
    transport_.disconnect();
    return false;
  }
  
  std::cout << "[MQTT] Connected!" << std::endl;
  sendAutoDiscovery();
  
  return true;
}

void MQTTService::threadLoop() {
  while (running_) {
    auto& cfg = ConfigManager::instance().getConfig();
    if (!cfg.mqttEnabled) {
      if (transport_.isConnected()) transport_.disconnect();
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    
    if (!transport_.isConnected()) {
      connect();
      if (!transport_.isConnected()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        continue;
      }
    }
    
    if (transport_.isConnected()) {
      mqtt_sync(&mqttClient_);
      if (mqttClient_.error != MQTT_OK) {
        std::cerr << "[MQTT] Error: " << mqtt_error_str(mqttClient_.error) << std::endl;
        transport_.disconnect();
      }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void MQTTService::sendAutoDiscovery() {
  auto& cfg = ConfigManager::instance().getConfig();
  if (cfg.mqttBaseTopic.empty()) return;
  
  std::string node_id = "hamclock_next";
  std::string state_topic = cfg.mqttBaseTopic + "/state";
  
  nlohmann::json device = {
    {"identifiers", {"hamclock_next_01"}},
    {"name", "HamClock-Next"},
    {"model", "HamClock-Next"},
    {"manufacturer", "Ziggy"}
  };
  
  // Helper lambda for creating sensor configs
  auto sendConfig = [&](const std::string& sensor_id, const std::string& name, const std::string& value_key) {
    nlohmann::json config = {
      {"name", name},
      {"state_topic", state_topic},
      {"value_template", "{{ value_json." + value_key + " }}"},
      {"unique_id", "hamclock_next_" + sensor_id},
      {"device", device}
    };
    publish("homeassistant/sensor/" + node_id + "/" + sensor_id + "/config", config.dump(), 1, true);
  };

  sendConfig("sfi", "Solar Flux Index", "sfi");
  sendConfig("sn", "Sunspot Number", "sn");
  sendConfig("a_index", "A-Index", "a_index");
  sendConfig("k_index", "K-Index", "k_index");
  sendConfig("xray", "X-Ray Flux", "xray");
  sendConfig("dx_spots", "Live DX Spots", "dx_spots");
}

void MQTTService::publish(const std::string &topic, const std::string &payload, int qos, bool retain) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!transport_.isConnected()) return;
  
  uint8_t flags = 0;
  if (retain) flags |= MQTT_PUBLISH_RETAIN;
  if (qos == 1) flags |= MQTT_PUBLISH_QOS_1;
  if (qos == 2) flags |= MQTT_PUBLISH_QOS_2;
  
  mqtt_publish(&mqttClient_, topic.c_str(), payload.c_str(), payload.length(), flags);
}

void MQTTService::publishState(struct AppContext& ctx) {
  uint32_t now = SDL_GetTicks();
  if (now - lastPublishTime_ < 60000) return; // Every 60s
  lastPublishTime_ = now;
  
  auto& cfg = ConfigManager::instance().getConfig();
  if (!cfg.mqttEnabled || cfg.mqttBaseTopic.empty() || !transport_.isConnected()) return;
  
  nlohmann::json state;
  
  if (ctx.solarStore) {
    auto solar = ctx.solarStore->get();
    if (solar.valid) {
      state["sfi"] = solar.sfi;
      state["sn"] = solar.sunspot_number;
      state["a_index"] = solar.a_index;
      state["k_index"] = solar.k_index;
      state["xray"] = solar.xray_flux;
    }
  }
  
  if (ctx.spotStore) {
    auto snapshot = ctx.spotStore->snapshot();
    if (snapshot) {
      state["dx_spots"] = snapshot->spots.size();
    }
  }
  
  std::string state_topic = cfg.mqttBaseTopic + "/state";
  publish(state_topic, state.dump(), 0, false);
}
