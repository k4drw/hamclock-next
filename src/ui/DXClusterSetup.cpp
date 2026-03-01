#include "DXClusterSetup.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>

DXClusterSetup::DXClusterSetup(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
  hostInput_.setMaxLength(64);
  portInput_.setMaxLength(5);
  loginInput_.setMaxLength(32);
  hostInput_.setActive(true);
  recalcLayout();
}

void DXClusterSetup::recalcLayout() {
  // Layout logic moved to render() for resolution independence
}

void DXClusterSetup::update() {}

TextInput &DXClusterSetup::activeInput() {
  switch (activeField_) {
  case 1:  return portInput_;
  case 2:  return loginInput_;
  default: return hostInput_;
  }
}

void DXClusterSetup::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Dark background with a slight fade to indicate it's an overlay
  SDL_SetRenderDrawColor(renderer, 10, 10, 20, 250);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  auto *cat = fontMgr_.catalog();
  int cx = x_ + width_ / 2;
  int pad = std::max(20, width_ / 20);
  int fieldW = std::min(500, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = cat->ptSize(FontStyle::UI) + 14;
  int textPad = 8;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {150, 150, 150, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  int y = y_ + height_ / 10;

  // --- Title ---
  cat->drawText(renderer, "DX Cluster Settings", cx, y, cyan,
                FontStyle::MediumBold, true);
  y += cat->ptSize(FontStyle::MediumBold) + pad;

  // --- Host & Port ---
  cat->drawText(renderer, "Cluster Host:", fieldX, y, white, FontStyle::UI);
  cat->drawText(renderer, "Port:", fieldX + fieldW - 100, y, white,
                FontStyle::UI);
  y += cat->ptSize(FontStyle::UI) + 4;

  int hostFieldW = fieldW - 110;
  hostInput_.render(renderer, fontMgr_, fieldX, y, hostFieldW, fieldH,
                    FontStyle::UI, textPad, activeField_ == 0, false,
                    orange, gray, white, white, gray, "e.g. dxc.k3lr.com");

  portInput_.render(renderer, fontMgr_, fieldX + fieldW - 100, y, 100, fieldH,
                    FontStyle::UI, textPad, activeField_ == 1, false,
                    orange, gray, white, white, gray, "7000");

  y += fieldH + pad;

  // --- Login ---
  cat->drawText(renderer, "Callsign / Login:", fieldX, y, white, FontStyle::UI);
  y += cat->ptSize(FontStyle::UI) + 4;
  loginInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                     FontStyle::UI, textPad, activeField_ == 2, false,
                     orange, gray, white, white, gray, "Your callsign");
  y += fieldH + pad;

  // --- UDP / WSJT-X ---
  toggleRect_ = {fieldX, y, 24, 24};
  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &toggleRect_);
  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
  SDL_RenderDrawRect(renderer, &toggleRect_);
  if (useWSJTX_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect inner = {toggleRect_.x + 4, toggleRect_.y + 4, 16, 16};
    SDL_RenderFillRect(renderer, &inner);
  }
  cat->drawText(renderer, "UDP Mode (receive from WSJT-X / JTDX)",
                fieldX + 35, y + 2, white, FontStyle::UI);
  y += pad * 2;

  // --- Buttons ---
  int btnW = 100;
  int btnH = 34;
  saveRect_ = {cx - btnW - pad / 2, y, btnW, btnH};
  cancelRect_ = {cx + pad / 2, y, btnW, btnH};

  // Done Button
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &saveRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
  SDL_RenderDrawRect(renderer, &saveRect_);
  cat->drawText(renderer, "Done", saveRect_.x + btnW / 2,
                saveRect_.y + btnH / 2, white, FontStyle::UIBold, true, false,
                true);

  // Cancel Button
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  cat->drawText(renderer, "Cancel", cancelRect_.x + btnW / 2,
                cancelRect_.y + btnH / 2, white, FontStyle::UIBold, true, false,
                true);

  y += btnH + pad;
  cat->drawText(renderer, "Tip: Tab rotates fields. Enter to Save.", cx, y,
                gray, FontStyle::Fast, true);
}

void DXClusterSetup::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool DXClusterSetup::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (mx >= toggleRect_.x && mx < toggleRect_.x + toggleRect_.w &&
      my >= toggleRect_.y && my < toggleRect_.y + toggleRect_.h) {
    useWSJTX_ = !useWSJTX_;
    return true;
  }

  if (mx >= saveRect_.x && mx < saveRect_.x + saveRect_.w &&
      my >= saveRect_.y && my < saveRect_.y + saveRect_.h) {
    complete_ = true;
    saved_ = true;
    return true;
  }

  if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
      my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
    complete_ = true;
    saved_ = false;
    return true;
  }

  // Field hit-test: recalculate the same geometry used in render()
  auto *cat = fontMgr_.catalog();
  int cx = x_ + width_ / 2;
  int pad = std::max(20, width_ / 20);
  int fieldW = std::min(500, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = cat->ptSize(FontStyle::UI) + 14;
  int uiSize = cat->ptSize(FontStyle::UI);
  int textPad = 8;

  int y = y_ + height_ / 10;
  y += cat->ptSize(FontStyle::MediumBold) + pad; // skip title
  y += uiSize + 4;                                // skip "Cluster Host:" label

  int hostFieldW = fieldW - 110;

  // Host field
  if (hostInput_.onMouseDown(mx, my, fontMgr_, fieldX, y, hostFieldW, fieldH,
                              FontStyle::UI, textPad)) {
    activeField_ = 0;
    hostInput_.setActive(true);
    portInput_.setActive(false);
    loginInput_.setActive(false);
    return true;
  }
  // Port field
  if (portInput_.onMouseDown(mx, my, fontMgr_, fieldX + fieldW - 100, y, 100,
                              fieldH, FontStyle::UI, textPad)) {
    activeField_ = 1;
    hostInput_.setActive(false);
    portInput_.setActive(true);
    loginInput_.setActive(false);
    return true;
  }

  y += fieldH + pad;            // advance past host/port row
  y += uiSize + 4;              // skip "Callsign / Login:" label

  // Login field
  if (loginInput_.onMouseDown(mx, my, fontMgr_, fieldX, y, fieldW, fieldH,
                               FontStyle::UI, textPad)) {
    activeField_ = 2;
    hostInput_.setActive(false);
    portInput_.setActive(false);
    loginInput_.setActive(true);
    return true;
  }

  return true;
}

bool DXClusterSetup::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (key == SDLK_TAB) {
    activeField_ = (activeField_ + 1) % kNumFields;
    hostInput_.setActive(activeField_ == 0);
    portInput_.setActive(activeField_ == 1);
    loginInput_.setActive(activeField_ == 2);
    activeInput().setCursorToEnd();
    return true;
  }

  if (key == SDLK_ESCAPE) {
    complete_ = true;
    saved_ = false;
    return true;
  }

  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    complete_ = true;
    saved_ = true;
    return true;
  }

  // Delegate navigation/edit keys to the active field
  if (activeInput().onKeyDown(key, mod))
    return true;

  return true;
}

bool DXClusterSetup::onTextInput(const char *inputText) {
  activeInput().onTextInput(inputText);
  return true;
}

void DXClusterSetup::setConfig(const AppConfig &cfg) {
  hostInput_.setValue(cfg.dxClusterHost);
  portInput_.setValue(std::to_string(cfg.dxClusterPort));
  loginInput_.setValue(cfg.dxClusterLogin);
  useWSJTX_ = cfg.dxClusterUseWSJTX;
  hostInput_.setCursorToEnd();
  hostInput_.setActive(true);
  portInput_.setActive(false);
  loginInput_.setActive(false);
  activeField_ = 0;
}

AppConfig DXClusterSetup::updateConfig(AppConfig cfg) const {
  cfg.dxClusterHost = hostInput_.getValue();
  cfg.dxClusterPort = std::atoi(portInput_.getValue().c_str());
  if (cfg.dxClusterPort == 0)
    cfg.dxClusterPort = 7300;
  cfg.dxClusterLogin = loginInput_.getValue();
  cfg.dxClusterUseWSJTX = useWSJTX_;
  return cfg;
}
