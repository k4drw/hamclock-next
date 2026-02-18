#pragma once

#include "../core/ContestData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <vector>

struct SDL_Renderer;

class ContestPanel : public Widget {
public:
  ContestPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<ContestStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool isModalActive() const override { return popupOpen_; }

  std::string getName() const override { return "ContestPanel"; }

private:
  void renderPopup(SDL_Renderer *renderer);

  FontManager &fontMgr_;
  std::shared_ptr<ContestStore> store_;
  ContestData currentData_;
  bool dataValid_ = false;

  int labelFontSize_ = 12;
  int itemFontSize_ = 10;

  // Popup state
  bool popupOpen_ = false;
  int selectedContestIdx_ = -1;        // index into currentData_.contests
  std::vector<int> displayedIndices_;  // maps visible row → contest index
  std::vector<SDL_Rect> rowRects_;     // screen rects of visible rows
};
