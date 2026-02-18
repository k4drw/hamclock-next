#include "SoundManager.h"
#include "Logger.h"
#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SoundManager &SoundManager::getInstance() {
  static SoundManager instance;
  return instance;
}

SoundManager::~SoundManager() { cleanup(); }

bool SoundManager::init() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_)
    return true;

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

void SoundManager::playAlarm() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_ || !alarmChunk_)
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
