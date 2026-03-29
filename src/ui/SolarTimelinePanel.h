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
  const char *typeId() const override { return "solar_timeline"; }
  std::string getDisplayName() const override { return "Solar Impact"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  // Async state shared with the background HTTP callback lambda.
  // Using shared_ptr means the lambda safely outlives this widget.
  struct AsyncState {
    SolarTimelineData data;
    std::mutex mutex;
    std::atomic<bool> fetching{false};
  };

  FontManager &fontMgr_;
  NetworkManager &net_;
  std::shared_ptr<AsyncState> async_ = std::make_shared<AsyncState>();
  uint32_t lastFetch_ = 0;

  SDL_Color kpColor(float kp) const;
};
