#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../services/AsteroidProvider.h"
#include "ListPanel.h"
#include <functional>
#include <memory>

class AsteroidPanel : public ListPanel {
public:
  AsteroidPanel(int x, int y, int w, int h, FontManager &fontMgr,
                AsteroidProvider &provider,
                std::shared_ptr<HamClockState> state = nullptr,
                AppConfig *config = nullptr,
                std::function<void()> onSave = nullptr);
  ~AsteroidPanel() override = default;

  void update() override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

  // Semantic Debug API
  std::string getName() const override { return "AsteroidPanel"; }

protected:
  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;

private:
  void rebuildRows();

  AsteroidProvider &provider_;
  AsteroidData lastData_;
  std::shared_ptr<HamClockState> state_;
  int selectedIndex_ = -1; // -1 = none; maps to asteroid index
  AppConfig *config_ = nullptr;
  std::function<void()> onSave_;
};
