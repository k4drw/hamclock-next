#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "SysInfoPanel.h"
#include "WidgetRegistry.h"
#include "../network/NetworkManager.h"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Static helper: local IPv4 address (loopback excluded)
// ---------------------------------------------------------------------------
std::string SysInfoPanel::getLocalIP() {
  return NetworkManager::getLocalIP();
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
static SDL_Color colorForTemp(float tempC, const ThemeColors &themes) {
  if (tempC < 50.0f)
    return themes.success; // Green
  if (tempC < 70.0f)
    return themes.warning; // Yellow
  if (tempC < 85.0f)
    return {255, 165, 0, 255}; // Orange
  return themes.danger;     // Red
}

static SDL_Color colorForCpu(float pct, const ThemeColors &themes) {
  if (pct < 50.0f)
    return themes.success; // Green
  if (pct < 75.0f)
    return themes.warning; // Yellow
  return themes.danger;    // Red
}

// ---------------------------------------------------------------------------
// render()
// ---------------------------------------------------------------------------
void SysInfoPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  bool tempAvail = monitor_ && monitor_->isAvailable();

  // Build common strings
  const char *tempUnit = useMetric_ ? "C" : "F";
  float tempC =
      useMetric_ ? currentTemp_ : (currentTemp_ - 32.0f) * 5.0f / 9.0f;
  SDL_Color tempColor = tempAvail ? colorForTemp(tempC, themes) : themes.textDim;
  SDL_Color cpuColor = colorForCpu(cpuPercent_, themes);

  char tempBuf[32], cpuBuf[32], ramBuf[48], vramBuf[32];
  if (tempAvail)
    std::snprintf(tempBuf, sizeof(tempBuf), "%.1f°%s", currentTemp_, tempUnit);
  else
    std::snprintf(tempBuf, sizeof(tempBuf), "--°%s", tempUnit);
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
  cat->drawText(renderer, ramBuf, cx, curY, themes.info, FontStyle::Fast, true);
  curY += cat->ptSize(FontStyle::Fast) + 4;

  // Row 3: Est. VRAM
  if (vramBytes_ >= 0) {
    float vramM = static_cast<float>(vramBytes_) / (1024.0f * 1024.0f);
    std::snprintf(vramBuf, sizeof(vramBuf), "VRAM ~%.0f MB", vramM);
  } else {
    std::snprintf(vramBuf, sizeof(vramBuf), "VRAM --");
  }
  cat->drawText(renderer, vramBuf, cx, curY, themes.textDim, FontStyle::Fast,
                true);
  curY += cat->ptSize(FontStyle::Fast) + 4;

  // Row 4: Disk
  if (diskPct_ >= 0) {
    char diskBuf[32];
    std::snprintf(diskBuf, sizeof(diskBuf), "Disk %d%%", diskPct_);
    cat->drawText(renderer, diskBuf, cx, curY, themes.info, FontStyle::Fast,
                  true);
    curY += cat->ptSize(FontStyle::Fast) + 4;
  }

  // Row 5: IP
  cat->drawText(renderer, cachedIP_.c_str(), cx, curY, themes.textDim,
                FontStyle::Fast, true);
  curY += cat->ptSize(FontStyle::Fast) + 4;

  // Row 6+: Provider errors (show up to 3 failing services)
  if (state_) {
    int shown = 0;
    std::lock_guard<std::mutex> lk(state_->servicesMutex);
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

REGISTER_WIDGET("sys_info", "System Info", false, false, {
  return std::make_unique<SysInfoPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.cpuMonitor, deps.state, deps.appCfg.useMetric);
})
