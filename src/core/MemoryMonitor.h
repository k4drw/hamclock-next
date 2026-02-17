#pragma once

#include "Logger.h"
#include <SDL.h>
#include <atomic>
#include <cstdio>
#include <string>

#if defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

class MemoryMonitor {
public:
  static MemoryMonitor &getInstance() {
    static MemoryMonitor instance;
    return instance;
  }

  void addVram(int64_t bytes) { vramBytes_ += bytes; }
  void markVramDestroyed(int64_t bytes) { vramBytes_ -= bytes; }
  int64_t getVramEstimated() const { return vramBytes_.load(); }

  // Safe wrapper for destroying textures with accurate VRAM tracking
  void destroyTexture(SDL_Texture *&tex) {
    if (!tex)
      return;
    int w, h;
    if (SDL_QueryTexture(tex, nullptr, nullptr, &w, &h) == 0) {
      markVramDestroyed(static_cast<int64_t>(w) * h * 4);
    }
    SDL_DestroyTexture(tex);
    tex = nullptr;
  }

  // Get Resident Set Size (RSS) in bytes
  size_t getRSS() {
#if defined(__linux__)
    long rss = 0L;
    FILE *fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
      return 0L;
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
      fclose(fp);
      return 0L;
    }
    fclose(fp);
    return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
      return pmc.WorkingSetSize;
    }
    return 0;
#else
    return 0;
#endif
  }

  void logStats(const std::string &context = "") {
    size_t rss = getRSS();
    int64_t vram = getVramEstimated();

    std::string ctxStr = context.empty() ? "" : "[" + context + "] ";
    LOG_I("Memory", "{}: SYS RSS: {:.2f} MB, Est. VRAM: {:.2f} MB", ctxStr,
          rss / 1024.0 / 1024.0, vram / 1024.0 / 1024.0);
  }

private:
  MemoryMonitor() : vramBytes_(0) {}
  std::atomic<int64_t> vramBytes_;
};
