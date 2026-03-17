#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/RigData.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Service for interfacing with Hamlib rigctld daemon.
// Implements async producer-consumer pattern for CAT command execution.
// Periodically polls rig state (freq, mode, signal level, VFO, RIT, split).
class RigService {
public:
  RigService(std::shared_ptr<RigDataStore> store, const AppConfig &config,
             HamClockState *state = nullptr);
  ~RigService();

  void start();
  void stop();

  // --- Control interface (producer: queues async commands) ---

  bool setFrequency(long long freqHz);
  bool setFrequencyB(long long freqHz);   // Set VFO-B frequency (Hamlib >= 3.3)
  bool setMode(const std::string &mode, int passbandHz = 0);
  bool setPTT(bool on);
  bool setVFO(bool vfoA);                 // Switch active VFO (true=A, false=B)
  bool setRIT(long long ritHz);           // Set RIT offset in Hz
  bool setSplit(bool on);                 // Enable/disable split (TX on VFO-B)
  bool vfoSwap();                         // Swap VFO-A and VFO-B frequencies

  // Non-blocking read of cached state
  RigData getState() const;

  bool isRunning() const { return running_; }
  bool isConnected() const;

private:
  std::shared_ptr<RigDataStore> store_;
  const AppConfig &config_;
  HamClockState *state_;

  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};

  std::queue<RigCommandRequest> commandQueue_;
  std::mutex queueMutex_;
  std::condition_variable queueCV_;
  std::thread workerThread_;
  std::mutex sockMutex_;

  int sockfd_ = -1;

  // Periodic poll tracking
  std::chrono::steady_clock::time_point lastPollTime_;
  bool spectrumProbed_ = false;
  static constexpr int POLL_INTERVAL_MS = 2000;
  static constexpr size_t MAX_QUEUE_SIZE = 100;

  void commandWorker();
  void pollRigState();

  bool connectToRig();
  void disconnectFromRig();
  bool sendCommand(const std::string &cmd, std::string &response);

  // --- Execute methods (consumer: called from worker thread) ---
  bool executeSetFreq(long long freqHz);
  bool executeGetFreq(long long &freqHz);
  bool executeSetFreqB(long long freqHz);
  bool executeGetFreqB(long long &freqHz);
  bool executeSetMode(const std::string &mode, int passbandHz);
  bool executeGetMode(std::string &mode, int &passbandHz);
  bool executeSetPTT(bool on);
  bool executeSetVFO(bool vfoA);
  bool executeGetVFO(bool &vfoA);
  bool executeSetRIT(long long ritHz);
  bool executeGetRIT(long long &ritHz);
  bool executeSetSplit(bool on);
  bool executeGetSplit(bool &split);
  bool executeGetLevel(float &dbm);
  bool executeVFOSwap();
  bool executeGetSpectrum(std::vector<float> &data); // Placeholder
};
