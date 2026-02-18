#pragma once
#include <SDL.h>
#include <SDL_mixer.h>
#include <mutex>

class SoundManager {
public:
  static SoundManager &getInstance();

  bool init();
  void cleanup();

  // Play the countdown alarm (a series of chirps)
  void playAlarm();

private:
  SoundManager() = default;
  ~SoundManager();

  bool initialized_ = false;
  Mix_Chunk *alarmChunk_ = nullptr;
  std::mutex mutex_;

  void createAlarmSound();
};
