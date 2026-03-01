#pragma once

#include "../core/SatelliteManager.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"
#include <string>

namespace HamClock {

class SatelliteSetup : public Widget {
public:
  SatelliteSetup(int x, int y, int w, int h, FontManager &fontMgr,
                 SatelliteManager &satMgr);
  ~SatelliteSetup() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override {
    Widget::onResize(x, y, w, h);
    calculateLayout();
  }
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onTextInput(const char *text) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

  bool isActive() const { return active_; }
  void setActive(bool active);

  bool isConfiguring() const override { return active_; }

  std::string getName() const override { return "SatelliteSetup"; }
  nlohmann::json getDebugData() const override;

private:
  bool active_ = false;
  FontManager &fontMgr_;
  SatelliteManager &satMgr_;

  enum class InputMode { None, SCC, TLE };
  InputMode inputMode_ = InputMode::None;
  TextInput sccInput_;
  std::string tleInput_;

  SDL_Rect rectOk_;
  SDL_Rect rectCancel_;
  SDL_Rect rectSccInput_;
  SDL_Rect rectTleInput_;
  SDL_Rect rectList_;
  SDL_Rect dialogRect_;

  void calculateLayout();
  void addSatelliteBySCC();
  void addSatelliteByTLE();
};

} // namespace HamClock
