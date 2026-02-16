#include "RotatorService.h"
#include "../core/Logger.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

RotatorService::RotatorService(std::shared_ptr<RotatorDataStore> store,
                               const AppConfig &config, HamClockState *state)
    : store_(std::move(store)), config_(config), state_(state) {}

RotatorService::~RotatorService() { stop(); }

void RotatorService::start() {
  if (running_)
    return;

  // Check if rotator is enabled in config
  if (config_.rotatorHost.empty() || config_.rotatorPort == 0) {
    LOG_I("Rotator", "Rotator not configured, service disabled");
    return;
  }

  running_ = true;
  pollThread_ = std::thread(&RotatorService::pollLoop, this);
  LOG_I("Rotator", "Service started ({}:{})", config_.rotatorHost,
        config_.rotatorPort);
}

void RotatorService::stop() {
  if (!running_)
    return;

  running_ = false;
  disconnectFromRotator();

  if (pollThread_.joinable()) {
    pollThread_.join();
  }

  LOG_I("Rotator", "Service stopped");
}

RotatorData RotatorService::getPosition() const {
  if (store_) {
    return store_->get();
  }
  return RotatorData{};
}

bool RotatorService::setPosition(double azimuth, double elevation) {
  if (!connected_) {
    LOG_W("Rotator", "Cannot set position: not connected");
    return false;
  }

  bool success = setAzEl(azimuth, elevation);

  if (success) {
    LOG_I("Rotator", "Position command sent: Az={:.1f} El={:.1f}", azimuth,
          elevation);

    // Update moving flag in store
    RotatorData data = store_->get();
    data.moving = true;
    store_->set(data);
  } else {
    LOG_E("Rotator", "Failed to set position");
  }

  return success;
}

bool RotatorService::stopRotator() {
  if (!connected_) {
    LOG_W("Rotator", "Cannot stop: not connected");
    return false;
  }

  std::string response;
  bool success = sendCommand("S\n", response);

  if (success) {
    LOG_I("Rotator", "Stop command sent");

    // Update moving flag
    RotatorData data = store_->get();
    data.moving = false;
    store_->set(data);
  }

  return success;
}

bool RotatorService::isConnected() const { return connected_.load(); }

void RotatorService::pollLoop() {
  using namespace std::chrono_literals;

  while (running_) {
    // Try to connect if not connected
    if (!connected_) {
      if (connectToRotator()) {
        LOG_I("Rotator", "Connected to rotctld");
        connected_ = true;

        if (state_) {
          state_->services["Rotator"].ok = true;
          state_->services["Rotator"].lastError = "";
        }
      } else {
        // Connection failed, retry after delay
        if (state_) {
          state_->services["Rotator"].ok = false;
          state_->services["Rotator"].lastError = "Connection failed";
        }
        std::this_thread::sleep_for(5s);
        continue;
      }
    }

    // Poll rotator position
    double az = 0, el = 0;
    if (getAzEl(az, el)) {
      RotatorData data;
      data.azimuth = az;
      data.elevation = el;
      data.connected = true;
      data.moving = false; // Will be set by movement detection or commands
      data.lastUpdate = std::chrono::system_clock::now();
      data.valid = true;

      store_->set(data);

      if (state_) {
        state_->services["Rotator"].ok = true;
        state_->services["Rotator"].lastSuccess =
            std::chrono::system_clock::now();
        state_->services["Rotator"].lastError = "";
      }
    } else {
      // Read failed, disconnect and retry
      LOG_W("Rotator", "Position query failed, reconnecting...");
      disconnectFromRotator();
      connected_ = false;

      if (state_) {
        state_->services["Rotator"].ok = false;
        state_->services["Rotator"].lastError = "Position query failed";
      }

      // Mark data as invalid
      RotatorData data = store_->get();
      data.connected = false;
      data.valid = false;
      store_->set(data);

      std::this_thread::sleep_for(5s);
      continue;
    }

    // Poll every 500ms (2 Hz update rate)
    std::this_thread::sleep_for(500ms);
  }

  disconnectFromRotator();
}

bool RotatorService::connectToRotator() {
  // Create TCP socket
  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ < 0) {
    LOG_E("Rotator", "Failed to create socket: {}", strerror(errno));
    return false;
  }

  // Set socket timeout
  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  // Connect to rotctld
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.rotatorPort);

  if (inet_pton(AF_INET, config_.rotatorHost.c_str(), &addr.sin_addr) <= 0) {
    LOG_E("Rotator", "Invalid address: {}", config_.rotatorHost);
    close(sockfd_);
    sockfd_ = -1;
    return false;
  }

  if (connect(sockfd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_E("Rotator", "Connection failed: {}", strerror(errno));
    close(sockfd_);
    sockfd_ = -1;
    return false;
  }

  return true;
}

void RotatorService::disconnectFromRotator() {
  if (sockfd_ >= 0) {
    close(sockfd_);
    sockfd_ = -1;
  }
  connected_ = false;
}

bool RotatorService::sendCommand(const std::string &cmd, std::string &response) {
  if (sockfd_ < 0) {
    return false;
  }

  // Send command
  ssize_t sent = send(sockfd_, cmd.c_str(), cmd.length(), 0);
  if (sent < 0) {
    LOG_E("Rotator", "Send failed: {}", strerror(errno));
    return false;
  }

  // Read response
  char buffer[256];
  std::memset(buffer, 0, sizeof(buffer));
  ssize_t received = recv(sockfd_, buffer, sizeof(buffer) - 1, 0);

  if (received <= 0) {
    LOG_E("Rotator", "Receive failed: {}", strerror(errno));
    return false;
  }

  response = std::string(buffer, received);
  return true;
}

bool RotatorService::getAzEl(double &az, double &el) {
  // Send 'p' command to get position
  std::string response;
  if (!sendCommand("p\n", response)) {
    return false;
  }

  // Parse response: "Azimuth: XXX.X\nElevation: YYY.Y\n" or just "XXX.X\nYYY.Y\n"
  // Hamlib rotctld returns: "Azimuth\nElevation\n" for 'p' command
  double azVal = 0, elVal = 0;

  // Try simple format first (most common)
  if (std::sscanf(response.c_str(), "%lf\n%lf", &azVal, &elVal) == 2) {
    az = azVal;
    el = elVal;
    return true;
  }

  // Try verbose format
  if (std::sscanf(response.c_str(), "Azimuth: %lf\nElevation: %lf", &azVal,
                  &elVal) == 2) {
    az = azVal;
    el = elVal;
    return true;
  }

  LOG_W("Rotator", "Failed to parse position response: {}", response);
  return false;
}

bool RotatorService::setAzEl(double az, double el) {
  // Send 'P' command to set position
  char cmd[64];
  std::snprintf(cmd, sizeof(cmd), "P %.1f %.1f\n", az, el);

  std::string response;
  if (!sendCommand(cmd, response)) {
    return false;
  }

  // Check for "RPRT 0" (success) in response
  if (response.find("RPRT 0") != std::string::npos || response.empty()) {
    return true;
  }

  LOG_W("Rotator", "Set position returned: {}", response);
  return false;
}
