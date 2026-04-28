#pragma once

#include "../core/FlareData.h"
#include "ListPanel.h"
#include "FontManager.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class FlareLogPanel : public ListPanel {
public:
  FlareLogPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<FlareDataStore> flareStore);
  ~FlareLogPanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "FlareLog"; }
  const char *typeId() const override { return "flare_log"; }
  std::string getDisplayName() const override { return "Solar Flares"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

private:
  std::shared_ptr<FlareDataStore> flareStore_;
};
