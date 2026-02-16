#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/RotatorData.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

// Service for interfacing with Hamlib rotctld daemon
// Implements async polling and position control via TCP
class RotatorService {
public:
  RotatorService(std::shared_ptr<RotatorDataStore> store,
                 const AppConfig &config, HamClockState *state = nullptr);
  ~RotatorService();

  // Start/stop background polling thread
  void start();
  void stop();

  // Get current rotator position (non-blocking, returns cached data)
  RotatorData getPosition() const;

  // Send azimuth/elevation command to rotator
  bool setPosition(double azimuth, double elevation);

  // Stop rotator movement
  bool stopRotator();

  // Check if service is running
  bool isRunning() const { return running_; }

  // Get connection status
  bool isConnected() const;

private:
  std::shared_ptr<RotatorDataStore> store_;
  const AppConfig &config_;
  HamClockState *state_;

  std::atomic<bool> running_{false};
  std::thread pollThread_;

  // Background polling loop
  void pollLoop();

  // Low-level Hamlib rotctld communication
  bool connectToRotator();
  void disconnectFromRotator();
  bool sendCommand(const std::string &cmd, std::string &response);
  bool getAzEl(double &az, double &el);
  bool setAzEl(double az, double el);

  // Socket file descriptor
  int sockfd_ = -1;
  std::atomic<bool> connected_{false};
};
