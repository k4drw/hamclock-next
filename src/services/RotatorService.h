#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/OrbitPredictor.h"
#include "../core/RotatorData.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

class Satellite; // Forward declaration

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

  // Auto-tracking control
  void autoTrack(const Satellite *sat);
  void stopAutoTrack();
  bool isAutoTracking() const;
  void setAutoTrackEnabled(bool enabled);
  bool getAutoTrackEnabled() const;

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

  // Auto-tracking state
  mutable std::mutex trackMutex_;
  bool autoTracking_ = false;
  const Satellite *currentSat_ = nullptr;
  OrbitPredictor
      predictor_; // Legacy: keeping for now to avoid breaking existing code
  SatelliteTLE currentTle_; // Legacy

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
