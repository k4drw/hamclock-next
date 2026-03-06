#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#endif

#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "SysInfoPanel.h"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

// ---------------------------------------------------------------------------
// Static helper: local IPv4 address (loopback excluded)
// ---------------------------------------------------------------------------
std::string SysInfoPanel::getLocalIP() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
  // GetAdaptersAddresses enumerates all adapters; pick first non-loopback IPv4
  ULONG bufLen = 15000;
  std::vector<BYTE> buf(bufLen);
  PIP_ADAPTER_ADDRESSES pAddrs = nullptr;
  ULONG ret;
  // Retry once if buffer is too small
  for (int attempt = 0; attempt < 2; ++attempt) {
    buf.resize(bufLen);
    pAddrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    ret = GetAdaptersAddresses(AF_INET,
                               GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                   GAA_FLAG_SKIP_DNS_SERVER,
                               nullptr, pAddrs, &bufLen);
    if (ret != ERROR_BUFFER_OVERFLOW)
      break;
  }
  if (ret != NO_ERROR)
    return "--";

  for (PIP_ADAPTER_ADDRESSES a = pAddrs; a; a = a->Next) {
    // Skip loopback, tunnel, and software-only adapters
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    if (a->IfType == IF_TYPE_TUNNEL)
      continue;
    if (a->OperStatus != IfOperStatusUp)
      continue;
    for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u;
         u = u->Next) {
      auto *sa = u->Address.lpSockaddr;
      if (sa->sa_family != AF_INET)
        continue;
      char ipBuf[INET_ADDRSTRLEN];
      auto *sin = reinterpret_cast<struct sockaddr_in *>(sa);
      if (inet_ntop(AF_INET, &sin->sin_addr, ipBuf, sizeof(ipBuf))) {
        return std::string(ipBuf);
      }
    }
  }
  return "--";
#elif defined(__EMSCRIPTEN__)
  return "WASM";
#else
  // POSIX: getifaddrs available on Linux and macOS
  struct ifaddrs *addrs = nullptr;
  if (getifaddrs(&addrs) != 0)
    return "--";
  std::string result = "--";
  for (struct ifaddrs *ifa = addrs; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;
    if (std::strcmp(ifa->ifa_name, "lo") == 0)
      continue;
    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
    result = ip;
    break;
  }
  freeifaddrs(addrs);
  return result;
#endif
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
SysInfoPanel::SysInfoPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<CPUMonitor> monitor,
                           std::shared_ptr<HamClockState> state,
                           bool useMetric)
    : Widget(x, y, w, h), fontMgr_(fontMgr), monitor_(std::move(monitor)),
      state_(std::move(state)), useMetric_(useMetric) {}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------
void SysInfoPanel::update() {
  uint32_t now = SDL_GetTicks();

  // Throttle stats updates to once per second
  if (lastStatsUpdateMs_ == 0 || (now - lastStatsUpdateMs_ >= 1000)) {
    auto &mem = MemoryMonitor::getInstance();

    // Temperature
    if (monitor_ && monitor_->isAvailable()) {
      currentTemp_ =
          useMetric_ ? monitor_->getTemperature() : monitor_->getTemperatureF();
    } else {
      currentTemp_ = 0.0f;
    }

    // CPU utilisation (always call even if temp unavailable — they're
    // independent)
    if (monitor_)
      cpuPercent_ = monitor_->getCpuPercent();

    // Memory stats
    rssBytes_ = mem.getRSS();
    totalRam_ = mem.getTotalRAM();
    vramBytes_ = mem.getVramEstimated();
    diskPct_ = mem.getDiskUsagePct();

    lastStatsUpdateMs_ = now;
  }

  // IP — refresh every 60 seconds
  if (cachedIP_.empty() || (now - lastIPRefreshMs_ > 60000)) {
    cachedIP_ = getLocalIP();
    lastIPRefreshMs_ = now;
  }
}

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------
static SDL_Color colorForTemp(float tempC) {
  if (tempC < 50.0f)
    return {0, 255, 0, 255}; // Green
  if (tempC < 70.0f)
    return {255, 255, 0, 255}; // Yellow
  if (tempC < 85.0f)
    return {255, 165, 0, 255}; // Orange
  return {255, 0, 0, 255};     // Red
}

static SDL_Color colorForCpu(float pct) {
  if (pct < 50.0f)
    return {0, 200, 100, 255}; // Green
  if (pct < 75.0f)
    return {255, 200, 0, 255}; // Yellow
  return {255, 60, 60, 255};   // Red
}

// ---------------------------------------------------------------------------
// render()
// ---------------------------------------------------------------------------
void SysInfoPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  if (!monitor_ || !monitor_->isAvailable()) {
    cat->drawText(renderer, "No CPU Temp", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  // Build common strings
  const char *tempUnit = useMetric_ ? "C" : "F";
  float tempC =
      useMetric_ ? currentTemp_ : (currentTemp_ - 32.0f) * 5.0f / 9.0f;
  SDL_Color tempColor = colorForTemp(tempC);
  SDL_Color cpuColor = colorForCpu(cpuPercent_);

  char tempBuf[32], cpuBuf[32], ramBuf[48], vramBuf[32];
  std::snprintf(tempBuf, sizeof(tempBuf), "%.1f°%s", currentTemp_, tempUnit);
  std::snprintf(cpuBuf, sizeof(cpuBuf), "CPU %.0f%%", cpuPercent_);

  // ---- Minimum layout (h < 70): just temperature -------------------------
  if (height_ < 70) {
    cat->drawText(renderer, "CPU", x_ + width_ / 2, y_ + 6, themes.accent,
                  FontStyle::Fast, true);
    cat->drawText(renderer, tempBuf, x_ + width_ / 2, y_ + height_ / 2 + 4,
                  tempColor, FontStyle::SmallBold, true);
    return;
  }

  int pad = std::max(4, static_cast<int>(width_ * 0.04f));
  int cx = x_ + width_ / 2;

  // Title bar for compact and tall layouts
  // Use themes.text instead of themes.accent for more reliable visibility
  // Shift it down slightly to ensure it's not clipped by border
  cat->drawText(renderer, "System Info", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);
  int titleShift = cat->ptSize(FontStyle::MicroBold) + 10;

  // ---- Compact layout (70 ≤ h < 120): temp+CPU% / IP --------------------
  if (height_ < 120) {
    int row1Y = y_ + pad + titleShift;
    int row2Y = row1Y + cat->ptSize(FontStyle::SmallBold) + 5;

    // Row 1: temp (left-ish), CPU% (right-ish)
    int halfW = width_ / 2;
    cat->drawText(renderer, tempBuf, x_ + halfW / 2, row1Y, tempColor,
                  FontStyle::SmallBold, true);
    cat->drawText(renderer, cpuBuf, x_ + halfW + halfW / 2, row1Y, cpuColor,
                  FontStyle::SmallBold, true);

    // Row 2: IP
    cat->drawText(renderer, cachedIP_.c_str(), cx, row2Y, themes.textDim,
                  FontStyle::Fast, true);
    return;
  }

  // ---- Tall layout (h ≥ 120): 4 rows ------------------------------------
  //  1. Temp + CPU%
  //  2. RAM used / total
  //  3. Est. VRAM
  //  4. Local IP
  int curY = y_ + pad + titleShift;

  // Row 1: Temp + CPU%
  int halfW = width_ / 2;
  cat->drawText(renderer, tempBuf, x_ + halfW / 2, curY, tempColor,
                FontStyle::SmallBold, true);
  cat->drawText(renderer, cpuBuf, x_ + halfW + halfW / 2, curY, cpuColor,
                FontStyle::SmallBold, true);
  curY += cat->ptSize(FontStyle::SmallBold) + 7;

  // Row 2: RAM
  if (totalRam_ > 0) {
    float rssM = static_cast<float>(rssBytes_) / (1024.0f * 1024.0f);
    float totalM = static_cast<float>(totalRam_) / (1024.0f * 1024.0f);
    std::snprintf(ramBuf, sizeof(ramBuf), "RAM %.0f/%.0f MB", rssM, totalM);
  } else {
    std::snprintf(ramBuf, sizeof(ramBuf), "RAM --");
  }
  cat->drawText(renderer, ramBuf, cx, curY, themes.info, FontStyle::Micro, true);
  curY += cat->ptSize(FontStyle::Micro) + 4;

  // Row 3: Est. VRAM
  if (vramBytes_ >= 0) {
    float vramM = static_cast<float>(vramBytes_) / (1024.0f * 1024.0f);
    std::snprintf(vramBuf, sizeof(vramBuf), "VRAM ~%.0f MB", vramM);
  } else {
    std::snprintf(vramBuf, sizeof(vramBuf), "VRAM --");
  }
  cat->drawText(renderer, vramBuf, cx, curY, themes.textDim, FontStyle::Micro,
                true);
  curY += cat->ptSize(FontStyle::Micro) + 4;

  // Row 4: Disk
  if (diskPct_ >= 0) {
    char diskBuf[32];
    std::snprintf(diskBuf, sizeof(diskBuf), "Disk %d%%", diskPct_);
    cat->drawText(renderer, diskBuf, cx, curY, themes.info, FontStyle::Micro,
                  true);
    curY += cat->ptSize(FontStyle::Micro) + 4;
  }

  // Row 5: IP
  cat->drawText(renderer, cachedIP_.c_str(), cx, curY, themes.textDim,
                FontStyle::Micro, true);
  curY += cat->ptSize(FontStyle::Micro) + 4;

  // Row 6+: Provider errors (show up to 3 failing services)
  if (state_) {
    int shown = 0;
    for (const auto &kv : state_->services) {
      if (curY + cat->ptSize(FontStyle::Micro) > y_ + height_ - 2)
        break;
      if (shown >= 3)
        break;
      const auto &svc = kv.second;
      if (!svc.ok && !svc.lastError.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s: %s", kv.first.c_str(),
                      svc.lastError.c_str());
        cat->drawText(renderer, buf, cx, curY, themes.danger,
                      FontStyle::Caption, true);
        curY += cat->ptSize(FontStyle::Caption) + 3;
        ++shown;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// onResize()
// ---------------------------------------------------------------------------
void SysInfoPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

// ---------------------------------------------------------------------------
// getActionRect()
// ---------------------------------------------------------------------------
SDL_Rect SysInfoPanel::getActionRect(const std::string &action) const {
  (void)action;
  return {x_, y_, width_, height_};
}
