#pragma once

#include "../core/CPUMonitor.h"
#include "../core/HamClockState.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>

struct SDL_Renderer;

// Displays CPU temperature, CPU%, RAM usage, and local IP address.
// Layout adapts to available height:
//   Tall (≥120px):   temp+CPU%, RAM used/total, est. VRAM, IP
//   Compact (<120px): temp+CPU% / IP
//   Minimum (<70px):  temp only (original behaviour)
class SysInfoPanel : public Widget {
public:
  SysInfoPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<CPUMonitor> monitor,
               std::shared_ptr<HamClockState> state,
               bool useMetric = true);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "System Info"; }
  const char *typeId() const override { return "sys_info"; }
  std::string getDisplayName() const override { return "System Info"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  std::vector<std::string> getActions() const override { return {}; }
  SDL_Rect getActionRect(const std::string &action) const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<CPUMonitor> monitor_;
  std::shared_ptr<HamClockState> state_;
  bool useMetric_;

  // Temperature
  float currentTemp_ = 0.0f;

  // CPU utilisation (0–100%)
  float cpuPercent_ = 0.0f;

  // Memory
  size_t rssBytes_ = 0;
  size_t totalRam_ = 0;
  int64_t vramBytes_ = 0;
  int diskPct_ = -1;

  // Throttling for stats (CPU, RAM, Temp)
  uint32_t lastStatsUpdateMs_ = 0;

  // Local IP (refreshed every 60 s)
  std::string cachedIP_;
  uint32_t lastIPRefreshMs_ = 0;

  // Helper
  static std::string getLocalIP();
};
