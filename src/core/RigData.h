#pragma once

#include <chrono>
#include <mutex>
#include <string>

// Rig command types for async execution
enum class RigCommand {
  SET_FREQ,    // Set VFO frequency
  GET_FREQ,    // Get VFO frequency
  SET_MODE,    // Set operating mode
  GET_MODE,    // Get operating mode
  SET_PTT,     // Set PTT state
  GET_PTT,     // Get PTT state
  DISCONNECT,  // Disconnect from rig
};

// Command structure for producer-consumer queue
struct RigCommandRequest {
  RigCommand command;
  long long freqHz = 0;        // For SET_FREQ
  std::string mode;             // For SET_MODE (USB, LSB, CW, FM, etc.)
  int passbandHz = 0;           // For SET_MODE
  bool ptt = false;             // For SET_PTT
  std::string customCmd;        // For raw Hamlib commands
};

// Rig state and status data
struct RigData {
  long long freqHz = 0;         // Current VFO frequency in Hz
  std::string mode = "USB";     // Current mode
  int passbandHz = 2400;        // Current passband width
  bool ptt = false;             // PTT state
  bool connected = false;       // Connection status
  std::chrono::system_clock::time_point lastUpdate;
  bool valid = false;
};

// Thread-safe data store for rig state
class RigDataStore {
public:
  RigDataStore() = default;

  void set(const RigData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  RigData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  // Update specific fields atomically
  void setFrequency(long long freqHz) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.freqHz = freqHz;
    data_.lastUpdate = std::chrono::system_clock::now();
  }

  void setConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.connected = connected;
    data_.valid = connected;
  }

private:
  mutable std::mutex mutex_;
  RigData data_;
};
