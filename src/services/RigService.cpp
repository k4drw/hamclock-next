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

// --- Producer methods ---

bool RigService::setFrequency(long long freqHz) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_FREQ;
  cmd.freqHz = freqHz;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::setFrequencyB(long long freqHz) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_FREQ_B;
  cmd.freqHz = freqHz;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::setMode(const std::string &mode, int passbandHz) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_MODE;
  cmd.mode = mode;
  cmd.passbandHz = passbandHz;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::setPTT(bool on) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_PTT;
  cmd.ptt = on;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)on;
  return false;
#endif
}

bool RigService::setVFO(bool vfoA) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_VFO;
  cmd.vfoA = vfoA;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)vfoA;
  return false;
#endif
}

bool RigService::setRIT(long long ritHz) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_RIT;
  cmd.ritHz = ritHz;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)ritHz;
  return false;
#endif
}

bool RigService::setSplit(bool on) {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::SET_SPLIT;
  cmd.splitOn = on;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  (void)on;
  return false;
#endif
}

bool RigService::vfoSwap() {
#ifndef __EMSCRIPTEN__
  if (!running_)
    return false;
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (commandQueue_.size() >= MAX_QUEUE_SIZE)
    return false;
  RigCommandRequest cmd;
  cmd.command = RigCommand::VFO_SWAP;
  commandQueue_.push(cmd);
  queueCV_.notify_one();
  return true;
#else
  return false;
#endif
}

RigData RigService::getState() const {
#ifndef __EMSCRIPTEN__
  if (store_)
    return store_->get();
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

// --- Worker thread ---

void RigService::commandWorker() {
#ifndef __EMSCRIPTEN__
  using namespace std::chrono_literals;

  LOG_I("Rig", "Command worker thread started");

  lastPollTime_ = std::chrono::steady_clock::now();
  spectrumProbed_ = false;

  if (connectToRig()) {
    LOG_I("Rig", "Connected to rigctld");
    connected_ = true;
    store_->setConnected(true);
    if (state_) {
      std::lock_guard<std::mutex> lk(state_->servicesMutex);
      state_->services["Rig"].ok = true;
      state_->services["Rig"].lastError = "";
    }
  } else {
    LOG_W("Rig", "Initial connection failed");
    connected_ = false;
    store_->setConnected(false);
    if (state_) {
      std::lock_guard<std::mutex> lk(state_->servicesMutex);
      state_->services["Rig"].ok = false;
      state_->services["Rig"].lastError = "Connection failed";
    }
  }

  while (running_) {
    RigCommandRequest cmd;

    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      if (!queueCV_.wait_for(lock, 1s,
                             [this] { return !commandQueue_.empty(); })) {
        // Timeout — no command pending; run periodic poll
        if (connected_) {
          auto now = std::chrono::steady_clock::now();
          auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - lastPollTime_)
                             .count();
          if (elapsed >= POLL_INTERVAL_MS) {
            pollRigState();
            lastPollTime_ = now;
          }
        }
        continue;
      }
      cmd = commandQueue_.front();
      commandQueue_.pop();
    }

    if (cmd.command == RigCommand::DISCONNECT) {
      LOG_I("Rig", "Received shutdown command");
      break;
    }

    if (!connected_) {
      LOG_W("Rig", "Not connected, attempting reconnection...");
      if (connectToRig()) {
        LOG_I("Rig", "Reconnected to rigctld");
        connected_ = true;
        store_->setConnected(true);
        spectrumProbed_ = false;
        if (state_) {
          std::lock_guard<std::mutex> lk(state_->servicesMutex);
          state_->services["Rig"].ok = true;
          state_->services["Rig"].lastError = "";
        }
      } else {
        LOG_E("Rig", "Reconnection failed, dropping command");
        std::this_thread::sleep_for(5s);
        continue;
      }
    }

    bool success = false;
    switch (cmd.command) {

    case RigCommand::SET_FREQ:
      success = executeSetFreq(cmd.freqHz);
      if (success)
        store_->setFrequency(cmd.freqHz);
      break;

    case RigCommand::SET_FREQ_B:
      success = executeSetFreqB(cmd.freqHz);
      if (success)
        store_->setFrequencyB(cmd.freqHz);
      break;

    case RigCommand::SET_MODE:
      success = executeSetMode(cmd.mode, cmd.passbandHz);
      if (success)
        store_->setMode(cmd.mode, cmd.passbandHz);
      break;

    case RigCommand::SET_PTT:
      success = executeSetPTT(cmd.ptt);
      break;

    case RigCommand::SET_VFO:
      success = executeSetVFO(cmd.vfoA);
      if (success)
        store_->setVFO(cmd.vfoA);
      break;

    case RigCommand::SET_RIT:
      success = executeSetRIT(cmd.ritHz);
      if (success)
        store_->setRIT(cmd.ritHz);
      break;

    case RigCommand::SET_SPLIT:
      success = executeSetSplit(cmd.splitOn);
      if (success)
        store_->setSplit(cmd.splitOn);
      break;

    case RigCommand::VFO_SWAP:
      success = executeVFOSwap();
      break;

    case RigCommand::GET_FREQ: {
      long long freq = 0;
      success = executeGetFreq(freq);
      if (success)
        store_->setFrequency(freq);
      break;
    }

    case RigCommand::GET_MODE: {
      std::string mode;
      int passband = 0;
      success = executeGetMode(mode, passband);
      if (success)
        store_->setMode(mode, passband);
      break;
    }

    default:
      LOG_W("Rig", "Unknown command type");
      success = true; // Don't disconnect on unknown cmd
      break;
    }

    if (!success) {
      LOG_E("Rig", "Command execution failed, disconnecting");
      disconnectFromRig();
      connected_ = false;
      store_->setConnected(false);
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->servicesMutex);
        state_->services["Rig"].ok = false;
        state_->services["Rig"].lastError = "Command execution failed";
      }
    }

    std::this_thread::sleep_for(10ms);
  }

  LOG_I("Rig", "Command worker thread exiting");
#endif
}

// Periodic rig state poll — called from worker thread.
// Failures on optional fields (level, VFO, RIT, split, spectrum) are
// non-fatal. Only core freq/mode failure returns early.
void RigService::pollRigState() {
#ifndef __EMSCRIPTEN__
  // Core: frequency
  long long freq = 0;
  if (!executeGetFreq(freq)) {
    LOG_W("Rig", "Poll: get_freq failed");
    return;
  }
  store_->setFrequency(freq);

  // Core: mode
  std::string mode;
  int passband = 0;
  if (executeGetMode(mode, passband))
    store_->setMode(mode, passband);

  // Optional: VFO-B frequency
  long long freqB = 0;
  if (executeGetFreqB(freqB))
    store_->setFrequencyB(freqB);

  // Optional: active VFO
  bool vfoA = true;
  if (executeGetVFO(vfoA))
    store_->setVFO(vfoA);

  // Optional: RIT offset
  long long rit = 0;
  if (executeGetRIT(rit))
    store_->setRIT(rit);

  // Optional: split state
  bool split = false;
  if (executeGetSplit(split))
    store_->setSplit(split);

  // Optional: signal level (S-meter)
  float level = -127.0f;
  if (executeGetLevel(level))
    store_->setSignalLevel(level);

  // Spectrum: probe once after connect; subsequent reads only if supported
  if (!spectrumProbed_) {
    std::vector<float> specData;
    bool ok = executeGetSpectrum(specData);
    store_->setSpectrum(specData, ok);
    spectrumProbed_ = true;
    LOG_I("Rig", "Spectrum support: {}", ok ? "yes" : "no");
  }
#endif
}

// --- Socket plumbing ---

bool RigService::connectToRig() {
#ifndef __EMSCRIPTEN__
  int newFd = (int)socket(AF_INET, SOCK_STREAM, 0);
  {
    std::lock_guard<std::mutex> lock(sockMutex_);
    sockfd_ = newFd;
  }
  if (newFd < 0) {
    LOG_E("Rig", "Failed to create socket");
    return false;
  }

#ifdef _WIN32
  DWORD timeout_ms = 2000;
  setsockopt(newFd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms,
             sizeof(timeout_ms));
  setsockopt(newFd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms,
             sizeof(timeout_ms));
#else
  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(newFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(newFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.rigPort);

  if (inet_pton(AF_INET, config_.rigHost.c_str(), &addr.sin_addr) <= 0) {
    LOG_E("Rig", "Invalid address: {}", config_.rigHost);
#ifdef _WIN32
    closesocket(newFd);
#else
    close(newFd);
#endif
    std::lock_guard<std::mutex> lock(sockMutex_);
    sockfd_ = -1;
    return false;
  }

  if (connect(newFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_E("Rig", "Connection failed");
#ifdef _WIN32
    closesocket(newFd);
#else
    close(newFd);
#endif
    std::lock_guard<std::mutex> lock(sockMutex_);
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
  {
    std::lock_guard<std::mutex> lock(sockMutex_);
    if (sockfd_ >= 0) {
#ifdef _WIN32
      closesocket(sockfd_);
#else
      close(sockfd_);
#endif
      sockfd_ = -1;
    }
  }
  connected_ = false;
#endif
}

bool RigService::sendCommand(const std::string &cmd, std::string &response) {
#ifndef __EMSCRIPTEN__
  int fd;
  {
    std::lock_guard<std::mutex> lock(sockMutex_);
    fd = sockfd_;
  }
  if (fd < 0)
    return false;

  ssize_t sent = send(fd, cmd.c_str(), cmd.length(), 0);
  if (sent < 0) {
    LOG_E("Rig", "Send failed: {}", strerror(errno));
    return false;
  }

  char buffer[512];
  std::memset(buffer, 0, sizeof(buffer));
  ssize_t received = recv(fd, buffer, sizeof(buffer) - 1, 0);

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

// --- Execute methods ---

bool RigService::executeSetFreq(long long freqHz) {
#ifndef __EMSCRIPTEN__
  char cmd[64];
  std::snprintf(cmd, sizeof(cmd), "F %lld\n", freqHz);
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeGetFreq(long long &freqHz) {
#ifndef __EMSCRIPTEN__
  std::string response;
  if (!sendCommand("f\n", response))
    return false;
  return std::sscanf(response.c_str(), "%lld", &freqHz) == 1;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeSetFreqB(long long freqHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib long-form: \set_freq VFOB <hz>  (requires rigctld >= 3.3)
  char cmd[80];
  std::snprintf(cmd, sizeof(cmd), "\\set_freq VFOB %lld\n", freqHz);
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeGetFreqB(long long &freqHz) {
#ifndef __EMSCRIPTEN__
  // Hamlib long-form: \get_freq VFOB  (requires rigctld >= 3.3)
  std::string response;
  if (!sendCommand("\\get_freq VFOB\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false; // Not supported by this rig/version
  return std::sscanf(response.c_str(), "%lld", &freqHz) == 1;
#else
  (void)freqHz;
  return false;
#endif
}

bool RigService::executeSetMode(const std::string &mode, int passbandHz) {
#ifndef __EMSCRIPTEN__
  char cmd[128];
  std::snprintf(cmd, sizeof(cmd), "M %s %d\n", mode.c_str(), passbandHz);
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::executeGetMode(std::string &mode, int &passbandHz) {
#ifndef __EMSCRIPTEN__
  std::string response;
  if (!sendCommand("m\n", response))
    return false;
  char modeStr[32];
  if (std::sscanf(response.c_str(), "%31s\n%d", modeStr, &passbandHz) == 2) {
    mode = modeStr;
    return true;
  }
  return false;
#else
  (void)mode;
  (void)passbandHz;
  return false;
#endif
}

bool RigService::executeSetPTT(bool on) {
#ifndef __EMSCRIPTEN__
  char cmd[16];
  std::snprintf(cmd, sizeof(cmd), "T %d\n", on ? 1 : 0);
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)on;
  return false;
#endif
}

bool RigService::executeSetVFO(bool vfoA) {
#ifndef __EMSCRIPTEN__
  std::string cmd = vfoA ? "V VFOA\n" : "V VFOB\n";
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)vfoA;
  return false;
#endif
}

bool RigService::executeGetVFO(bool &vfoA) {
#ifndef __EMSCRIPTEN__
  std::string response;
  if (!sendCommand("v\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false;
  vfoA = (response.find("VFOA") != std::string::npos);
  return true;
#else
  (void)vfoA;
  return false;
#endif
}

bool RigService::executeSetRIT(long long ritHz) {
#ifndef __EMSCRIPTEN__
  char cmd[32];
  std::snprintf(cmd, sizeof(cmd), "J %lld\n", ritHz);
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)ritHz;
  return false;
#endif
}

bool RigService::executeGetRIT(long long &ritHz) {
#ifndef __EMSCRIPTEN__
  std::string response;
  if (!sendCommand("j\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false;
  return std::sscanf(response.c_str(), "%lld", &ritHz) == 1;
#else
  (void)ritHz;
  return false;
#endif
}

bool RigService::executeSetSplit(bool on) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'S' (set_split_vfo): "S <split> <VFO>"
  std::string cmd = on ? "S 1 VFOB\n" : "S 0 VFOA\n";
  std::string response;
  if (!sendCommand(cmd, response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  (void)on;
  return false;
#endif
}

bool RigService::executeGetSplit(bool &split) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'x' (get_split_vfo): response is "<0|1>\n<VFO>\n"
  std::string response;
  if (!sendCommand("x\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false;
  int val = 0;
  if (std::sscanf(response.c_str(), "%d", &val) == 1) {
    split = (val != 0);
    return true;
  }
  return false;
#else
  (void)split;
  return false;
#endif
}

bool RigService::executeGetLevel(float &dbm) {
#ifndef __EMSCRIPTEN__
  // Hamlib 'l' (get_level): "l STRENGTH\n" → single float in dBm
  std::string response;
  if (!sendCommand("l STRENGTH\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false;
  return std::sscanf(response.c_str(), "%f", &dbm) == 1;
#else
  (void)dbm;
  return false;
#endif
}

bool RigService::executeVFOSwap() {
#ifndef __EMSCRIPTEN__
  // Hamlib 'G' (vfo_op): "G XCHG\n" swaps VFO-A and VFO-B frequencies
  std::string response;
  if (!sendCommand("G XCHG\n", response))
    return false;
  return response.find("RPRT 0") != std::string::npos || response.empty();
#else
  return false;
#endif
}

bool RigService::executeGetSpectrum(std::vector<float> &data) {
#ifndef __EMSCRIPTEN__
  // Placeholder: Hamlib get_spectrum_line is only available on a small set
  // of rigs (e.g. IC-7300 via proprietary CAT, some SDR-based rigs).
  // Response format when supported: "<center_hz> <span_hz> <len> <vals...>\n"
  // Returns false if rig does not support spectrum (RPRT error or no parse).
  std::string response;
  if (!sendCommand("\\get_spectrum_line\n", response))
    return false;
  if (response.find("RPRT") != std::string::npos)
    return false;
  // TODO: parse space-separated float values after header when rig confirmed
  (void)data;
  return false;
#else
  (void)data;
  return false;
#endif
}
