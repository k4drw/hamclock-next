#include "RigService.h"
#include "../core/Logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <chrono>
#include <cstring>
#include <thread>

RigService::RigService(std::shared_ptr<RigDataStore> store,
                       const AppConfig &config, HamClockState *state)
    : store_(std::move(store)), config_(config), state_(state) {}

RigService::~RigService() { stop(); }

void RigService::start() {
#ifndef __EMSCRIPTEN__
  if (running_)
    return;

  // Check if rig is enabled in config
  if (config_.rigHost.empty() || config_.rigPort == 0) {
    LOG_I("Rig", "Rig not configured, service disabled");
    return;
  }

  running_ = true;
  workerThread_ = std::thread(&RigService::commandWorker, this);
  LOG_I("Rig", "Service started ({}:{})", config_.rigHost, config_.rigPort);
#endif
}

void RigService::stop() {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return;

  running_ = false;

  // Wake up worker thread and signal shutdown
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    RigCommandRequest shutdownCmd;
    shutdownCmd.command = RigCommand::DISCONNECT;
    commandQueue_.push(shutdownCmd);
  }
  queueCV_.notify_one();

  if (workerThread_.joinable()) {
    workerThread_.join();
  }

  disconnectFromRig();
  LOG_I("Rig", "Service stopped");
#endif
}

bool RigService::setFrequency(long long freqHz) {
#ifndef __EMSCRIPTEN__
  if (!running_) {
    LOG_W("Rig", "Service not running, cannot set frequency");
    return false;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);

  // Check queue depth to prevent memory growth
  if (commandQueue_.size() >= MAX_QUEUE_SIZE) {
    LOG_W("Rig", "Command queue full, dropping SET_FREQ command");
    return false;
  }

  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_FREQ;
  cmd.freqHz = freqHz;

  commandQueue_.push(cmd);
  queueCV_.notify_one();

  LOG_I("Rig", "Queued SET_FREQ: {} Hz", freqHz);
  return true;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::setMode(const std::string &mode, int passbandHz) {
#ifndef __EMSCRIPTEN__
  if (!running_) {
    LOG_W("Rig", "Service not running, cannot set mode");
    return false;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);

  if (commandQueue_.size() >= MAX_QUEUE_SIZE) {
    LOG_W("Rig", "Command queue full, dropping SET_MODE command");
    return false;
  }

  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_MODE;
  cmd.mode = mode;
  cmd.passbandHz = passbandHz;

  commandQueue_.push(cmd);
  queueCV_.notify_one();

  LOG_I("Rig", "Queued SET_MODE: {} ({}Hz)", mode, passbandHz);
  return true;
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::setPTT(bool on) {
#ifndef __EMSCRIPTEN__
  if (!running_) {
    LOG_W("Rig", "Service not running, cannot set PTT");
    return false;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);

  if (commandQueue_.size() >= MAX_QUEUE_SIZE) {
    LOG_W("Rig", "Command queue full, dropping SET_PTT command");
    return false;
  }

  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_PTT;
  cmd.ptt = on;

  commandQueue_.push(cmd);
  queueCV_.notify_one();

  LOG_I("Rig", "Queued SET_PTT: {}", on ? "ON" : "OFF");
  return true;
#else
  (void)on;
  return false;
#endif
}

RigData RigService::getState() const {
#ifndef __EMSCRIPTEN__
  if (store_) {
    return store_->get();
  }
#endif
  return RigData{};
}

bool RigService::isConnected() const {
#ifndef __EMSCRIPTEN__
  return connected_.load();
#else
  return false;
#endif
}

void RigService::commandWorker() {
#ifndef __EMSCRIPTEN__
  using namespace std::chrono_literals;

  LOG_I("Rig", "Command worker thread started");

  // Initial connection attempt
  if (connectToRig()) {
    LOG_I("Rig", "Connected to rigctld");
    connected_ = true;
    store_->setConnected(true);

    if (state_) {
      state_->services["Rig"].ok = true;
      state_->services["Rig"].lastError = "";
    }
  } else {
    LOG_W("Rig", "Initial connection failed");
    connected_ = false;
    store_->setConnected(false);

    if (state_) {
      state_->services["Rig"].ok = false;
      state_->services["Rig"].lastError = "Connection failed";
    }
  }

  while (running_) {
    RigCommandRequest cmd;

    // Wait for command (with timeout for periodic health checks)
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      if (queueCV_.wait_for(lock, 5s,
                            [this] { return !commandQueue_.empty(); })) {
        cmd = commandQueue_.front();
        commandQueue_.pop();
      } else {
        // Timeout - no command received
        // Could do periodic health check here if needed
        continue;
      }
    }

    // Handle DISCONNECT command (shutdown signal)
    if (cmd.command == RigCommand::DISCONNECT) {
      LOG_I("Rig", "Received shutdown command");
      break;
    }

    // Ensure we're connected before executing commands
    if (!connected_) {
      LOG_W("Rig", "Not connected, attempting reconnection...");
      if (connectToRig()) {
        LOG_I("Rig", "Reconnected to rigctld");
        connected_ = true;
        store_->setConnected(true);

        if (state_) {
          state_->services["Rig"].ok = true;
          state_->services["Rig"].lastError = "";
        }
      } else {
        LOG_E("Rig", "Reconnection failed, dropping command");
        std::this_thread::sleep_for(5s);
        continue;
      }
    }

    // Execute command
    bool success = false;
    switch (cmd.command) {
    case RigCommand::SET_FREQ:
      success = executeSetFreq(cmd.freqHz);
      if (success) {
        store_->setFrequency(cmd.freqHz);
        if (state_) {
          state_->services["Rig"].lastSuccess =
              std::chrono::system_clock::now();
        }
      }
      break;

    case RigCommand::SET_MODE:
      success = executeSetMode(cmd.mode, cmd.passbandHz);
      break;

    case RigCommand::SET_PTT:
      success = executeSetPTT(cmd.ptt);
      break;

    case RigCommand::GET_FREQ: {
      long long freq = 0;
      success = executeGetFreq(freq);
      if (success) {
        store_->setFrequency(freq);
      }
      break;
    }

    case RigCommand::GET_MODE: {
      std::string mode;
      int passband = 0;
      success = executeGetMode(mode, passband);
      break;
    }

    default:
      LOG_W("Rig", "Unknown command type");
      break;
    }

    // Handle command failure
    if (!success) {
      LOG_E("Rig", "Command execution failed, disconnecting");
      disconnectFromRig();
      connected_ = false;
      store_->setConnected(false);

      if (state_) {
        state_->services["Rig"].ok = false;
        state_->services["Rig"].lastError = "Command execution failed";
      }
    }

    // Small delay to prevent tight loops
    std::this_thread::sleep_for(10ms);
  }

  LOG_I("Rig", "Command worker thread exiting");
#endif
}

bool RigService::connectToRig() {
#ifndef __EMSCRIPTEN__
  // Create TCP socket
  sockfd_ = (int)socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ < 0) {
    LOG_E("Rig", "Failed to create socket");
    return false;
  }

  // Set socket timeout (platform-specific)
#ifdef _WIN32
  DWORD timeout_ms = 2000;
  setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms,
             sizeof(timeout_ms));
  setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms,
             sizeof(timeout_ms));
#else
  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

  // Connect to rigctld
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.rigPort);

  if (inet_pton(AF_INET, config_.rigHost.c_str(), &addr.sin_addr) <= 0) {
    LOG_E("Rig", "Invalid address: {}", config_.rigHost);
#ifdef _WIN32
    closesocket(sockfd_);
#else
    close(sockfd_);
#endif
    sockfd_ = -1;
    return false;
  }

  if (connect(sockfd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_E("Rig", "Connection failed");
#ifdef _WIN32
    closesocket(sockfd_);
#else
    close(sockfd_);
#endif
    sockfd_ = -1;
    return false;
  }

  return true;
#else
  return false;
#endif
}

void RigService::disconnectFromRig() {
#ifndef __EMSCRIPTEN__
  if (sockfd_ >= 0) {
#ifdef _WIN32
    closesocket(sockfd_);
#else
    close(sockfd_);
#endif
    sockfd_ = -1;
  }
  connected_ = false;
#endif
}

bool RigService::sendCommand(const std::string &cmd, std::string &response) {
#ifndef __EMSCRIPTEN__
  if (sockfd_ < 0) {
    return false;
  }

  // Send command
  ssize_t sent = send(sockfd_, cmd.c_str(), cmd.length(), 0);
  if (sent < 0) {
    LOG_E("Rig", "Send failed: {}", strerror(errno));
    return false;
  }

  // Read response
  char buffer[512];
  std::memset(buffer, 0, sizeof(buffer));
  ssize_t received = recv(sockfd_, buffer, sizeof(buffer) - 1, 0);

  if (received <= 0) {
    LOG_E("Rig", "Receive failed: {}", strerror(errno));
    return false;
  }

  response = std::string(buffer, received);
  return true;
#else
  (void)cmd;
  (void)response;
  return false;
#endif
}

bool RigService::executeSetFreq(long long freqHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'F' command: Set frequency in Hz
  char cmd[64];
  std::snprintf(cmd, sizeof(cmd), "F %lld\n", freqHz);

  std::string response;
  if (!sendCommand(cmd, response)) {
    return false;
  }

  // Check for "RPRT 0" (success) in response
  if (response.find("RPRT 0") != std::string::npos || response.empty()) {
    LOG_I("Rig", "Frequency set to {} Hz", freqHz);
    return true;
  }

  LOG_W("Rig", "Set frequency returned: {}", response);
  return false;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeGetFreq(long long &freqHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'f' command: Get frequency
  std::string response;
  if (!sendCommand("f\n", response)) {
    return false;
  }

  // Parse response (frequency in Hz as single number)
  if (std::sscanf(response.c_str(), "%lld", &freqHz) == 1) {
    LOG_I("Rig", "Frequency read: {} Hz", freqHz);
    return true;
  }

  LOG_W("Rig", "Failed to parse frequency response: {}", response);
  return false;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeSetMode(const std::string &mode, int passbandHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'M' command: Set mode and passband
  // Format: M <mode> <passband>
  char cmd[128];
  std::snprintf(cmd, sizeof(cmd), "M %s %d\n", mode.c_str(), passbandHz);

  std::string response;
  if (!sendCommand(cmd, response)) {
    return false;
  }

  // Check for success
  if (response.find("RPRT 0") != std::string::npos || response.empty()) {
    LOG_I("Rig", "Mode set to {} ({}Hz)", mode, passbandHz);
    return true;
  }

  LOG_W("Rig", "Set mode returned: {}", response);
  return false;
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::executeGetMode(std::string &mode, int &passbandHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'm' command: Get mode
  std::string response;
  if (!sendCommand("m\n", response)) {
    return false;
  }

  // Parse response: "MODE\nPASSBAND\n"
  char modeStr[32];
  if (std::sscanf(response.c_str(), "%s\n%d", modeStr, &passbandHz) == 2) {
    mode = modeStr;
    LOG_I("Rig", "Mode read: {} ({}Hz)", mode, passbandHz);
    return true;
  }

  LOG_W("Rig", "Failed to parse mode response: {}", response);
  return false;
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::executeSetPTT(bool on) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'T' command: Set PTT
  // Format: T <0|1>
  char cmd[16];
  std::snprintf(cmd, sizeof(cmd), "T %d\n", on ? 1 : 0);

  std::string response;
  if (!sendCommand(cmd, response)) {
    return false;
  }

  // Check for success
  if (response.find("RPRT 0") != std::string::npos || response.empty()) {
    LOG_I("Rig", "PTT set to {}", on ? "ON" : "OFF");
    return true;
  }

  LOG_W("Rig", "Set PTT returned: {}", response);
  return false;
#else
  (void)on;
  return false;
#endif
}
