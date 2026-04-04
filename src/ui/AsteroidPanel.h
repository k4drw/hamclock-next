#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../services/AsteroidProvider.h"
#include "ListPanel.h"
#include <functional>
#include <memory>
#include <vector>

class AsteroidProvider;
class TextureManager;

class AsteroidPanel : public ListPanel {
public:
  AsteroidPanel(int x, int y, int w, int h, FontManager &fontMgr,
                TextureManager &texMgr, AsteroidProvider &provider,
                std::shared_ptr<HamClockState> state = nullptr,
                AppConfig *config = nullptr,
                std::function<void()> onSave = nullptr);
  ~AsteroidPanel() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseWheel(int scrollY) override;

  // Semantic Debug API
  std::string getName() const override { return "AsteroidPanel"; }
  const char *typeId() const override { return "asteroid"; }
  std::string getDisplayName() const override { return "Asteroids"; }

protected:
  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;
  void renderRowText(SDL_Renderer *renderer, int index, int rx, int ry, int rw,
                     int rh, SDL_Color color) override;

private:
  void rebuildRows();
  void renderPolarPlot(SDL_Renderer *renderer, float cx, float cy, int radius);

  struct AzElPoint { double az; double el; };

  TextureManager &texMgr_;
  AsteroidProvider &provider_;
  AsteroidData lastData_;
  std::shared_ptr<HamClockState> state_;
  int selectedIndex_ = -1;           // -1 = none; maps to asteroid index in lastData_
  std::vector<int> allRowToAstIndex_; // all display rows before scroll slicing
  std::vector<int> rowToAstIndex_;    // visible slice → lastData_ asteroid index
  int scrollOffset_ = 0;
  static constexpr int MAX_VISIBLE_ROWS = 7;
  AppConfig *config_ = nullptr;
  std::function<void()> onSave_;

  // Polar plot track (pre-computed in update)
  std::vector<AzElPoint> asteroidTrack_;
  AzElPoint asteroidCurrentAzEl_ = {0, 0};
  bool asteroidAboveHorizon_ = false;
};
