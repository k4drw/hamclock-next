#pragma once
#include <SDL.h>
#include <SDL_mixer.h>
#include <mutex>

class SoundManager {
public:
  static SoundManager &getInstance();

  bool init();
  void cleanup();

  // Permanently disable audio (e.g. --no-audio flag). Prevents Mix_OpenAudio
  // from ever being called, so hardware audio devices are never activated.
  void disable();

  // Play the countdown alarm (a series of chirps)
  void playAlarm();

private:
  SoundManager() = default;
  ~SoundManager();

  bool initialized_ = false;
  bool disabled_ = false;
  Mix_Chunk *alarmChunk_ = nullptr;
  std::mutex mutex_;

  void createAlarmSound();
};
