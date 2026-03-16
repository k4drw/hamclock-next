#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../network/NetworkManager.h"
#include <ctime>
#include <mutex>
#include <vector>

struct KpForecastBlock {
  time_t timeUtc = 0; // UTC start of 3-hour block
  float kp = 0.0f;   // predicted Kp index
};

struct SolarTimelineData {
  std::vector<KpForecastBlock> blocks;
  time_t fetchedAt = 0;
  bool valid = false;
};

class SolarTimelinePanel : public Widget {
public:
  SolarTimelinePanel(int x, int y, int w, int h, FontManager &fontMgr,
                     NetworkManager &net);

  std::string getName() const override { return "Solar Impact"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  NetworkManager &net_;
  SolarTimelineData data_;
  mutable std::mutex mutex_;
  uint32_t lastFetch_ = 0;
  bool fetching_ = false;

  SDL_Color kpColor(float kp) const;
};
