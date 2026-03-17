#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// Rig command types for async execution
enum class RigCommand {
  SET_FREQ,     // Set VFO-A frequency
  GET_FREQ,     // Get VFO-A frequency
  SET_MODE,     // Set operating mode
  GET_MODE,     // Get operating mode
  SET_PTT,      // Set PTT state
  GET_PTT,      // Get PTT state
  SET_FREQ_B,   // Set VFO-B frequency (Hamlib >= 3.3)
  GET_FREQ_B,   // Get VFO-B frequency
  SET_VFO,      // Set active VFO (A or B)
  GET_VFO,      // Get active VFO
  SET_RIT,      // Set RIT offset in Hz
  GET_RIT,      // Get RIT offset
  SET_SPLIT,    // Enable/disable split operation
  GET_SPLIT,    // Get split state
  GET_LEVEL,    // Get signal level (S-meter, Hamlib LEVEL_STRENGTH)
  VFO_SWAP,     // Swap VFO A and B (Hamlib vfo_op XCHG)
  GET_SPECTRUM, // Spectrum line readout (placeholder: limited rig support)
  DISCONNECT,   // Disconnect from rig
};

// Command structure for producer-consumer queue
struct RigCommandRequest {
  RigCommand command;
  long long freqHz = 0;      // SET_FREQ, SET_FREQ_B
  std::string mode;           // SET_MODE
  int passbandHz = 0;         // SET_MODE
  bool ptt = false;           // SET_PTT
  bool splitOn = false;       // SET_SPLIT
  long long ritHz = 0;        // SET_RIT
  bool vfoA = true;           // SET_VFO (true=VFOA, false=VFOB)
  std::string customCmd;      // Reserved for raw Hamlib commands
};

// Rig state and status data
struct RigData {
  long long freqHz = 0;         // VFO-A frequency in Hz
  std::string mode = "USB";     // Current mode (USB, LSB, CW, CWR, AM, FM, DIG, PKT)
  int passbandHz = 2400;        // Passband width in Hz
  bool ptt = false;             // PTT state
  bool connected = false;       // Connection status
  std::chrono::system_clock::time_point lastUpdate;
  bool valid = false;

  // Extended state (polled periodically when connected)
  float signalLevelDbm = -127.0f;  // S-meter in dBm (Hamlib LEVEL_STRENGTH)
  long long freqHzB = 0;           // VFO-B frequency in Hz
  bool vfoA = true;                 // true = VFO-A is active transmit/receive VFO
  long long ritHz = 0;             // RIT offset in Hz (signed)
  bool split = false;              // Split operation active (TX on VFO-B)

  // Spectrum (placeholder: only available on select rigs via Hamlib get_spectrum_line)
  std::vector<float> spectrum;     // Normalized amplitude values [0.0..1.0]
  bool spectrumSupported = false;  // Whether rig reported spectrum support
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

  void setMode(const std::string &mode, int passbandHz) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.mode = mode;
    data_.passbandHz = passbandHz;
  }

  void setSignalLevel(float dbm) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.signalLevelDbm = dbm;
  }

  void setFrequencyB(long long freqHz) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.freqHzB = freqHz;
  }

  void setVFO(bool vfoA) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.vfoA = vfoA;
  }

  void setRIT(long long ritHz) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.ritHz = ritHz;
  }

  void setSplit(bool split) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.split = split;
  }

  void setSpectrum(const std::vector<float> &data, bool supported) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.spectrum = data;
    data_.spectrumSupported = supported;
  }

private:
  mutable std::mutex mutex_;
  RigData data_;
};
