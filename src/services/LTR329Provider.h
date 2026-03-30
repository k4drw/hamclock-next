#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// Reads ambient light level from an LTR329 I2C sensor (address 0x29).
// Provides a relative lux value updated approximately every second.
// Used by BrightnessManager for photosensor auto-dim on supported hardware.
class LTR329Provider {
public:
  LTR329Provider();
  ~LTR329Provider();

  void start();
  void stop();

  bool isAvailable() const { return available_; }

  // Returns the last-read ambient light level in lux (CH0 raw, simplified).
  float getLux() const { return lux_.load(); }

private:
  void worker();
  bool initI2C();
  float readLux();

  std::atomic<bool> running_{false};
  std::atomic<bool> available_{false};
  std::atomic<float> lux_{0.0f};
  std::mutex stopMutex_;
  std::condition_variable stopCv_;
  std::thread thread_;
  int fd_ = -1;
};
