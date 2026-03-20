#pragma once
#include <SDL.h>
#include <SDL_mixer.h>
#include <mutex>
#include <string>
#ifdef HAMCLOCK_AUDIO_TTS
#include <condition_variable>
#include <queue>
#include <thread>
#endif

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

  // Speak text via flite TTS on a background worker thread.
  // No-op if HAMCLOCK_AUDIO_TTS is not compiled in or SoundManager is disabled.
  void speak(std::string text);

private:
  SoundManager() = default;
  ~SoundManager();

  bool initialized_ = false;
  bool disabled_ = false;
  Mix_Chunk *alarmChunk_ = nullptr;
  std::mutex mutex_;

  void createAlarmSound();

#ifdef HAMCLOCK_AUDIO_TTS
  void ttsWorkerFunc();
  std::thread ttsThread_;
  std::queue<std::string> ttsQueue_;
  std::mutex ttsMutex_;
  std::condition_variable ttsCv_;
  bool ttsShutdown_ = false;
  bool ttsThreadStarted_ = false;
#endif
};
