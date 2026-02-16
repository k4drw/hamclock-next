#pragma once

#include "../core/BandConditionsData.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <memory>

class BandConditionsPanel : public Widget {
public:
  BandConditionsPanel(int x, int y, int w, int h, FontManager &fontMgr,
                      std::shared_ptr<BandConditionsStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  // Semantic Debug API
  std::string getName() const override { return "BandConditions"; }
  std::vector<std::string> getActions() const override { return {}; }
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<BandConditionsStore> store_;
  BandConditionsData currentData_;
  bool dataValid_ = false;

  SDL_Color colorForCondition(BandCondition cond);
  const char *stringForCondition(BandCondition cond, bool shortForm = false) const;

  int labelFontSize_ = 12;
  int tableFontSize_ = 10;
};
