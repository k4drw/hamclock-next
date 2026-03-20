#include "SoundManager.h"
#include "Logger.h"
#include <cmath>
#include <cstring>
#include <vector>

#ifdef HAMCLOCK_AUDIO_TTS
#  if defined(_WIN32)
#    include <windows.h>
#    include <sapi.h>
// MXE cross-compile: sapi.lib not in toolchain; define GUIDs directly.
// Values from Microsoft SAPI 5.4 spec.
// {96749377-3391-11D2-9EE3-00C04F797396}
extern "C" const CLSID CLSID_SpVoice = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
// {6C44DF74-72B9-4992-A1EC-EF996E0422D4}
extern "C" const IID IID_ISpVoice = {0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}};
#  elif defined(__APPLE__)
#    include <spawn.h>
#    include <sys/wait.h>
#  else
#    include <flite/flite.h>
extern "C" cst_voice *register_cmu_us_kal16(const char *voxdir);
#  endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SoundManager &SoundManager::getInstance() {
  static SoundManager instance;
  return instance;
}

SoundManager::~SoundManager() { cleanup(); }

void SoundManager::disable() {
  std::lock_guard<std::mutex> lock(mutex_);
  disabled_ = true;
  if (alarmChunk_) {
    Mix_FreeChunk(alarmChunk_);
    alarmChunk_ = nullptr;
  }
  if (initialized_) {
    Mix_CloseAudio();
    initialized_ = false;
  }
#ifdef HAMCLOCK_AUDIO_TTS
  {
    std::lock_guard<std::mutex> lk(ttsMutex_);
    ttsShutdown_ = true;
    while (!ttsQueue_.empty()) ttsQueue_.pop();
  }
  ttsCv_.notify_one();
  if (ttsThread_.joinable()) ttsThread_.join();
#endif
}

bool SoundManager::init() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_)
    return true;
  if (disabled_)
    return false;

  // Initialize SDL audio subsystem if not already done
  if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
      LOG_E("SoundManager", "SDL_InitSubSystem(AUDIO) failed: {}",
            SDL_GetError());
      return false;
    }
  }

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
    LOG_E("SoundManager", "SDL_mixer OpenAudio failed: {}", Mix_GetError());
    return false;
  }

  createAlarmSound();
  initialized_ = true;
  return true;
}

void SoundManager::cleanup() {
#ifdef HAMCLOCK_AUDIO_TTS
  {
    std::lock_guard<std::mutex> lk(ttsMutex_);
    ttsShutdown_ = true;
    while (!ttsQueue_.empty()) ttsQueue_.pop();
  }
  ttsCv_.notify_one();
  if (ttsThread_.joinable()) ttsThread_.join();
#endif
  std::lock_guard<std::mutex> lock(mutex_);
  if (alarmChunk_) {
    Mix_FreeChunk(alarmChunk_);
    alarmChunk_ = nullptr;
  }
  if (initialized_) {
    Mix_CloseAudio();
    initialized_ = false;
  }
}

void SoundManager::speak(std::string text) {
#ifdef HAMCLOCK_AUDIO_TTS
  if (disabled_) return;
  {
    std::lock_guard<std::mutex> lk(ttsMutex_);
    if (!ttsThreadStarted_) {
      ttsThreadStarted_ = true;
      ttsThread_ = std::thread(&SoundManager::ttsWorkerFunc, this);
    }
    ttsQueue_.push(std::move(text));
  }
  ttsCv_.notify_one();
#endif
}

#ifdef HAMCLOCK_AUDIO_TTS
void SoundManager::ttsWorkerFunc() {
#if defined(_WIN32)
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ISpVoice *voice = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                                IID_ISpVoice, reinterpret_cast<void **>(&voice));
  if (FAILED(hr)) {
    LOG_E("SoundManager", "SAPI: CoCreateInstance failed: 0x{:08X}", (unsigned)hr);
    CoUninitialize();
    return;
  }
#elif defined(__APPLE__)
  // macOS: /usr/bin/say spawned per utterance — no persistent init needed
#else
  flite_init();
  cst_voice *voice = register_cmu_us_kal16(nullptr);
  if (!voice) {
    LOG_E("SoundManager", "flite: failed to init voice");
    return;
  }
#endif

  while (true) {
    std::unique_lock<std::mutex> lk(ttsMutex_);
    ttsCv_.wait(lk, [this] { return !ttsQueue_.empty() || ttsShutdown_; });
    if (ttsShutdown_ && ttsQueue_.empty()) break;
    std::string text = std::move(ttsQueue_.front());
    ttsQueue_.pop();
    lk.unlock();

#if defined(_WIN32)
    std::wstring wtext(text.begin(), text.end());
    voice->Speak(wtext.c_str(), SPF_DEFAULT, nullptr);
#elif defined(__APPLE__)
    extern char **environ;
    char *argv[] = {const_cast<char *>("/usr/bin/say"),
                    const_cast<char *>(text.c_str()), nullptr};
    pid_t pid;
    if (posix_spawn(&pid, "/usr/bin/say", nullptr, nullptr, argv, environ) == 0)
      waitpid(pid, nullptr, 0);
#else
    flite_text_to_speech(text.c_str(), voice, "play");
#endif
  }

#if defined(_WIN32)
  voice->Release();
  CoUninitialize();
#elif defined(__APPLE__)
  // nothing to clean up
#else
  delete_voice(voice);
#endif
}
#endif

void SoundManager::playAlarm() {
  // Lazy init: open audio device only when a sound is actually needed.
  // This prevents Mix_OpenAudio() from activating HDMI audio hardware at
  // startup, which causes noise on displays with built-in buzzer speakers.
  if (!initialized_) {
    if (!init())
      return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!alarmChunk_)
    return;
  Mix_PlayChannel(-1, alarmChunk_, 0);
}

void SoundManager::createAlarmSound() {
  // Generate a 0.5s "chime" (1000Hz sine wave decaying)
  const int sampleRate = 44100;
  const float duration = 0.5f;
  const int numSamples = static_cast<int>(sampleRate * duration);
  const int numChannels = 2; // Stereo
  const int bitsPerSample = 16;
  const int dataSize = numSamples * numChannels * (bitsPerSample / 8);

  struct WavHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t format;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
  };

  WavHeader h;
  std::memcpy(h.riff, "RIFF", 4);
  h.fileSize = sizeof(WavHeader) + dataSize - 8;
  std::memcpy(h.wave, "WAVE", 4);
  std::memcpy(h.fmt, "fmt ", 4);
  h.fmtSize = 16;
  h.format = 1; // PCM
  h.channels = numChannels;
  h.sampleRate = sampleRate;
  h.bitsPerSample = bitsPerSample;
  h.byteRate = h.sampleRate * h.channels * (h.bitsPerSample / 8);
  h.blockAlign = h.channels * (h.bitsPerSample / 8);
  std::memcpy(h.data, "data", 4);
  h.dataSize = dataSize;

  std::vector<uint8_t> wavBuffer(sizeof(WavHeader) + dataSize);
  std::memcpy(wavBuffer.data(), &h, sizeof(WavHeader));

  int16_t *dataPtr =
      reinterpret_cast<int16_t *>(wavBuffer.data() + sizeof(WavHeader));
  for (int i = 0; i < numSamples; ++i) {
    float t = static_cast<float>(i) / sampleRate;
    float envelope =
        std::exp(-4.0f * t); // Slightly gentler decay than previous idea
    float wave = std::sin(2.0f * M_PI * 880.0f * t); // A5 note
    int16_t sample = static_cast<int16_t>(wave * envelope * 16384.0f);
    dataPtr[i * 2] = sample;
    dataPtr[i * 2 + 1] = sample;
  }

  SDL_RWops *rw = SDL_RWFromMem(wavBuffer.data(), wavBuffer.size());
  alarmChunk_ = Mix_LoadWAV_RW(rw, 1);
  if (!alarmChunk_) {
    LOG_E("SoundManager", "Failed to load procedurally generated alarm: {}",
          Mix_GetError());
  }
}
