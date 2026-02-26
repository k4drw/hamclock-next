#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Manual declaration to bypass broken PSAPI headers in some MinGW environments
typedef struct _HC_PROCESS_MEMORY_COUNTERS {
  DWORD cb;
  DWORD PageFaultCount;
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
} HC_PROCESS_MEMORY_COUNTERS;

extern "C" {
// Use GetProcessMemoryInfo which is usually exported from psapi.dll
// We use the 'K32' prefix as well if it's modern Windows
BOOL WINAPI GetProcessMemoryInfo(HANDLE Process,
                                 HC_PROCESS_MEMORY_COUNTERS *ppsmemCounters,
                                 DWORD cb);
// Also common in modern psapi
BOOL WINAPI K32GetProcessMemoryInfo(HANDLE Process,
                                    HC_PROCESS_MEMORY_COUNTERS *ppsmemCounters,
                                    DWORD cb);
}
#endif

#include "Logger.h"
#include <SDL.h>
#include <atomic>
#include <cstdio>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/statvfs.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
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

  // Get current disk usage percentage (-1 if unavailable)
  int getDiskUsagePct() {
#if defined(_WIN32)
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes,
        totalNumberOfFreeBytes;
    // Passing NULL to GetDiskFreeSpaceEx defaults to the current volume
    if (GetDiskFreeSpaceExA(NULL, &freeBytesAvailable, &totalNumberOfBytes,
                            &totalNumberOfFreeBytes)) {
      double total = static_cast<double>(totalNumberOfBytes.QuadPart);
      double free = static_cast<double>(totalNumberOfFreeBytes.QuadPart);
      if (total > 0) {
        return static_cast<int>(100.0 * (1.0 - free / total));
      }
    }
    return -1;
#elif defined(__EMSCRIPTEN__)
    return -1;
#else
    struct statvfs stat;
    if (statvfs("/", &stat) != 0)
      return -1;
    double total = static_cast<double>(stat.f_blocks) * stat.f_frsize;
    double avail = static_cast<double>(stat.f_bavail) * stat.f_frsize;
    return (total > 0) ? static_cast<int>(100.0 * (1.0 - avail / total)) : 0;
#endif
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
#elif defined(__APPLE__)
    // Use mach task_info to get resident set size
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info),
                  &count) == KERN_SUCCESS) {
      return static_cast<size_t>(info.resident_size);
    }
    return 0;
#elif defined(_WIN32)
    HC_PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(pmc);
    // Try K32 first, then fallback
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
      return (size_t)pmc.WorkingSetSize;
    } else if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
      return (size_t)pmc.WorkingSetSize;
    }
    return 0;
#else
    return 0;
#endif
  }

  // Get total physical RAM in bytes
  size_t getTotalRAM() {
#if defined(__linux__)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    return (size_t)pages * (size_t)page_size;
#elif defined(__APPLE__)
    // sysctlbyname("hw.memsize") returns uint64_t total physical bytes
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) == 0) {
      return static_cast<size_t>(memsize);
    }
    return 0;
#elif defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
      return (size_t)status.ullTotalPhys;
    }
    return 0;
#else
    return 0;
#endif
  }

  bool isLowMemoryDevice() {
    size_t total = getTotalRAM();
    if (total == 0)
      return false; // Unknown, assume normal
    // Threshold: 1.5 GB. RPi3B has 1GB, RPi4 has 1, 2, 4 or 8GB.
    return total < (1536ULL * 1024 * 1024);
  }

  void logStats(const std::string &context = "") {
    size_t rss = getRSS();
    int64_t vram = getVramEstimated();
    size_t total = getTotalRAM();

    std::string ctxStr = context.empty() ? "" : "[" + context + "] ";
    LOG_I("Memory",
          "{}: SYS RSS: {:.2f} MB, Est. VRAM: {:.2f} MB, Total RAM: {:.2f} MB",
          ctxStr, rss / 1024.0 / 1024.0, vram / 1024.0 / 1024.0,
          total / 1024.0 / 1024.0);
  }

private:
  MemoryMonitor() : vramBytes_(0) {}
  std::atomic<int64_t> vramBytes_;
};
