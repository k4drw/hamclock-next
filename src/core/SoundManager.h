#pragma once
#include <SDL.h>
#include <SDL_mixer.h>
#include <atomic>
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

  // Suppress TTS while the display is off.  Call setScreenOn(false) when the
  // screen is powered off, setScreenOn(true) when it comes back on.
  void setScreenOn(bool on) { screenOn_.store(on, std::memory_order_relaxed); }
  bool isScreenOn() const { return screenOn_.load(std::memory_order_relaxed); }

  // Runtime mute toggle (persisted in config). Silences TTS and alarm chime.
  void setMuted(bool m) { muted_.store(m, std::memory_order_relaxed); }
  bool isMuted() const { return muted_.load(std::memory_order_relaxed); }

  void setVolume(int vol) { volume_.store(vol, std::memory_order_relaxed); }
  int getVolume() const { return volume_.load(std::memory_order_relaxed); }

private:
  SoundManager() = default;
  ~SoundManager();

  bool initialized_ = false;
  bool disabled_ = false;
  std::atomic<bool> screenOn_{true};
  std::atomic<bool> muted_{false};
  std::atomic<int> volume_{100};
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
