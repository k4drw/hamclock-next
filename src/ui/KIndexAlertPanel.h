#pragma once

#include "../core/ConfigManager.h"
#include "../core/KIndexHistoryData.h"
#include "../core/SolarData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class KIndexAlertPanel : public Widget {
public:
  KIndexAlertPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   std::shared_ptr<KIndexHistoryStore> store,
                   std::shared_ptr<SolarDataStore> solarStore,
                   const AppConfig *config);

  std::string getName() const override { return "KIndex Alert"; }
  const char *typeId() const override { return "kindex_trend"; }
  std::string getDisplayName() const override { return "K-Index Alert"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  // Color for a given Kp value: green (0-2), yellow (3-4), orange (5), red (6+)
  static SDL_Color kpColor(float kp);

  FontManager &fontMgr_;
  std::shared_ptr<KIndexHistoryStore> store_;
  std::shared_ptr<SolarDataStore> solarStore_;
  const AppConfig *config_;
};
