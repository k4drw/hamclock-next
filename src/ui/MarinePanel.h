#pragma once

#include "../core/MarineData.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"
#include <SDL.h>
#include <memory>
#include <string>

struct ThemeColors;
struct SDL_Renderer;

class MarinePanel : public Widget {
public:
  MarinePanel(int x, int y, int w, int h, FontManager &fontMgr,
              std::shared_ptr<MarineStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  void onMouseMove(int mx, int my) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  bool isModalActive() const override { return menuVisible_; }
  void renderModal(SDL_Renderer *renderer) override;

  std::string getName() const override { return "Marine"; }

private:
  void renderMenu(SDL_Renderer *renderer, const struct ThemeColors &themes);
  void recalcMenuLayout();
  void saveSettings();

  FontManager &fontMgr_;
  std::shared_ptr<MarineStore> store_;
  MarineData currentData_;

  // Modal / Config state
  bool menuVisible_ = false;
  TextInput stationInput_;
  TextInput buoyInput_;
  int activeField_ = 0; // 0 = station, 1 = buoy

  // Layout for Modal
  SDL_Rect menuRect_ = {0, 0, 0, 0};
  SDL_Rect stationRect_ = {0, 0, 0, 0};
  SDL_Rect buoyRect_ = {0, 0, 0, 0};
  SDL_Rect okRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};

  int titleFontSize_ = 12;
  int rowFontSize_ = 11;
  int subFontSize_ = 9;
  int menuFontSize_ = 16;
};
