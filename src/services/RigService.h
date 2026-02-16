#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/RigData.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Service for interfacing with Hamlib rigctld daemon
// Implements async producer-consumer pattern for CAT command execution
class RigService {
public:
  RigService(std::shared_ptr<RigDataStore> store, const AppConfig &config,
             HamClockState *state = nullptr);
  ~RigService();

  // Start/stop background command processing thread
  void start();
  void stop();

  // High-level CAT command interface (producer)
  // These methods queue commands for async execution

  // Set radio frequency (in Hz)
  bool setFrequency(long long freqHz);

  // Set operating mode (USB, LSB, CW, FM, AM, etc.)
  bool setMode(const std::string &mode, int passbandHz = 0);

  // Set PTT state
  bool setPTT(bool on);

  // Get current rig state (non-blocking, returns cached data)
  RigData getState() const;

  // Check if service is running
  bool isRunning() const { return running_; }

  // Check if connected to rig
  bool isConnected() const;

private:
  std::shared_ptr<RigDataStore> store_;
  const AppConfig &config_;
  HamClockState *state_;

  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};

  // Producer-consumer queue infrastructure
  std::queue<RigCommandRequest> commandQueue_;
  std::mutex queueMutex_;
  std::condition_variable queueCV_;
  std::thread workerThread_;

  // Worker thread that processes commands from queue
  void commandWorker();

  // Low-level Hamlib rigctld communication
  bool connectToRig();
  void disconnectFromRig();
  bool sendCommand(const std::string &cmd, std::string &response);

  // Command execution (consumer)
  bool executeSetFreq(long long freqHz);
  bool executeGetFreq(long long &freqHz);
  bool executeSetMode(const std::string &mode, int passbandHz);
  bool executeGetMode(std::string &mode, int &passbandHz);
  bool executeSetPTT(bool on);

  // Socket file descriptor
  int sockfd_ = -1;

  // Maximum queue depth (prevent memory growth)
  static constexpr size_t MAX_QUEUE_SIZE = 100;
};
