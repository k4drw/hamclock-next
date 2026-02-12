#include "DXClusterSetup.h"
#include <algorithm>
#include <cstring>

DXClusterSetup::DXClusterSetup(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
  recalcLayout();
}

void DXClusterSetup::recalcLayout() {
  int h = height_;
  titleSize_ = std::clamp(static_cast<int>(h * 0.08f), 20, 48);
  labelSize_ = std::clamp(static_cast<int>(h * 0.05f), 14, 24);
  fieldSize_ = std::clamp(static_cast<int>(h * 0.06f), 16, 32);
  hintSize_ = std::clamp(static_cast<int>(h * 0.04f), 12, 18);
}

void DXClusterSetup::update() {}

static void renderField(SDL_Renderer *renderer, FontManager &fontMgr,
                        const std::string &text, const std::string &placeholder,
                        int fieldX, int &y, int fieldW, int fieldH,
                        int fieldSize, int textPad, bool active, int cursorPos,
                        SDL_Color activeBorder, SDL_Color inactiveBorder,
                        SDL_Color textColor, SDL_Color placeholderColor) {
  SDL_Color border = active ? activeBorder : inactiveBorder;

  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_Rect rect = {fieldX, y, fieldW, fieldH};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (!text.empty()) {
    fontMgr.drawText(renderer, text, fieldX + textPad, y + textPad, textColor,
                     fieldSize);
  } else if (!active) {
    fontMgr.drawText(renderer, placeholder, fieldX + textPad, y + textPad,
                     placeholderColor, fieldSize);
  }

  if (active) {
    int cursorX = fieldX + textPad;
    if (cursorPos > 0 && !text.empty()) {
      TTF_Font *font = fontMgr.getFont(fieldSize);
      if (font) {
        std::string before = text.substr(0, cursorPos);
        int tw = 0, th = 0;
        TTF_SizeText(font, before.c_str(), &tw, &th);
        cursorX += tw;
      }
    }
    if ((SDL_GetTicks() / 500) % 2 == 0) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderDrawLine(renderer, cursorX, y + 4, cursorX, y + fieldH - 4);
    }
  }

  y += fieldH;
}

void DXClusterSetup::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Dark background with a slight fade to indicate it's an overlay
  SDL_SetRenderDrawColor(renderer, 10, 10, 20, 250);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  int cx = x_ + width_ / 2;
  int pad = std::max(20, width_ / 20);
  int fieldW = std::min(500, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;
  int textPad = 8;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {150, 150, 150, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  int y = y_ + height_ / 10;

  // --- Title ---
  {
    TTF_Font *font = fontMgr_.getFont(titleSize_);
    if (font) {
      int tw = 0, th = 0;
      TTF_SizeText(font, "DX Cluster Settings", &tw, &th);
      fontMgr_.drawText(renderer, "DX Cluster Settings", cx - tw / 2, y, cyan,
                        titleSize_, true);
      y += th + pad;
    }
  }

  // --- Host & Port ---
  fontMgr_.drawText(renderer, "Cluster Host:", fieldX, y, white, labelSize_,
                    true);
  fontMgr_.drawText(renderer, "Port:", fieldX + fieldW - 100, y, white,
                    labelSize_, true);
  y += labelSize_ + 4;

  int hostY = y;
  renderField(renderer, fontMgr_, hostText_, "e.g. dxc.k3lr.com", fieldX, hostY,
              fieldW - 110, fieldH, fieldSize_, textPad, activeField_ == 0,
              cursorPos_, orange, gray, white, gray);

  int portY = y;
  renderField(renderer, fontMgr_, portText_, "7000", fieldX + fieldW - 100,
              portY, 100, fieldH, fieldSize_, textPad, activeField_ == 1,
              cursorPos_, orange, gray, white, gray);

  y = std::max(hostY, portY) + pad;

  // --- Login ---
  fontMgr_.drawText(renderer, "Callsign / Login:", fieldX, y, white, labelSize_,
                    true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, loginText_, "Your callsign", fieldX, y,
              fieldW, fieldH, fieldSize_, textPad, activeField_ == 2,
              cursorPos_, orange, gray, white, gray);
  y += pad;

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
  fontMgr_.drawText(renderer, "UDP Mode (receive from WSJT-X / JTDX)",
                    fieldX + 35, y + 2, white, labelSize_);
  y += pad * 2;

  // --- Buttons ---
  int btnW = 120;
  int btnH = 40;
  saveRect_ = {cx - btnW - pad / 2, y, btnW, btnH};
  cancelRect_ = {cx + pad / 2, y, btnW, btnH};

  // Save Button
  SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
  SDL_RenderFillRect(renderer, &saveRect_);
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
  SDL_RenderDrawRect(renderer, &saveRect_);
  int tw, th;
  TTF_Font *btnFont = fontMgr_.getFont(labelSize_);
  TTF_SizeText(btnFont, "SAVE", &tw, &th);
  fontMgr_.drawText(renderer, "SAVE", saveRect_.x + (btnW - tw) / 2,
                    saveRect_.y + (btnH - th) / 2, white, labelSize_);

  // Cancel Button
  SDL_SetRenderDrawColor(renderer, 100, 0, 0, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  TTF_SizeText(btnFont, "CANCEL", &tw, &th);
  fontMgr_.drawText(renderer, "CANCEL", cancelRect_.x + (btnW - tw) / 2,
                    cancelRect_.y + (btnH - th) / 2, white, labelSize_);

  y += btnH + pad;
  fontMgr_.drawText(renderer, "Tip: Tab rotates fields. Enter to Save.",
                    cx - 150, y, gray, hintSize_);
}

void DXClusterSetup::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool DXClusterSetup::onMouseUp(int mx, int my, Uint16) {
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

  // Check fields
  int cx = x_ + width_ / 2;
  int pad = std::max(20, width_ / 20);
  int fieldW = std::min(500, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;

  int y = y_ + height_ / 10;
  // Skip title
  TTF_Font *font = fontMgr_.getFont(titleSize_);
  if (font) {
    int tw = 0, th = 0;
    TTF_SizeText(font, "DX Cluster Settings", &tw, &th);
    y += th + pad;
  }

  // Check Host (Field 0)
  if (mx >= fieldX && mx < fieldX + fieldW - 110 && my >= y + labelSize_ + 4 &&
      my < y + labelSize_ + 4 + fieldH) {
    activeField_ = 0;
    cursorPos_ = hostText_.size();
    return true;
  }
  // Check Port (Field 1)
  if (mx >= fieldX + fieldW - 100 && mx < fieldX + fieldW &&
      my >= y + labelSize_ + 4 && my < y + labelSize_ + 4 + fieldH) {
    activeField_ = 1;
    cursorPos_ = portText_.size();
    return true;
  }

  int hostY = y + labelSize_ + 4 + fieldH;
  int portY = y + labelSize_ + 4 + fieldH;
  y = std::max(hostY, portY) + pad;

  // Check Login (Field 2)
  if (mx >= fieldX && mx < fieldX + fieldW && my >= y + labelSize_ + 4 &&
      my < y + labelSize_ + 4 + fieldH) {
    activeField_ = 2;
    cursorPos_ = loginText_.size();
    return true;
  }

  return true;
}

bool DXClusterSetup::onKeyDown(SDL_Keycode key, Uint16) {
  std::string *text = nullptr;
  int maxLen = 0;
  switch (activeField_) {
  case 0:
    text = &hostText_;
    maxLen = 64;
    break;
  case 1:
    text = &portText_;
    maxLen = 5;
    break;
  case 2:
    text = &loginText_;
    maxLen = 32;
    break;
  }
  if (!text)
    return true;

  if (key == SDLK_TAB) {
    activeField_ = (activeField_ + 1) % kNumFields;
    switch (activeField_) {
    case 0:
      cursorPos_ = hostText_.size();
      break;
    case 1:
      cursorPos_ = portText_.size();
      break;
    case 2:
      cursorPos_ = loginText_.size();
      break;
    }
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

  if (key == SDLK_BACKSPACE && cursorPos_ > 0) {
    text->erase(cursorPos_ - 1, 1);
    --cursorPos_;
    return true;
  }

  if (key == SDLK_DELETE && cursorPos_ < (int)text->size()) {
    text->erase(cursorPos_, 1);
    return true;
  }

  if (key == SDLK_LEFT && cursorPos_ > 0) {
    cursorPos_--;
    return true;
  }
  if (key == SDLK_RIGHT && cursorPos_ < (int)text->size()) {
    cursorPos_++;
    return true;
  }

  if (key == SDLK_HOME) {
    cursorPos_ = 0;
    return true;
  }
  if (key == SDLK_END) {
    cursorPos_ = text->size();
    return true;
  }

  return true;
}

bool DXClusterSetup::onTextInput(const char *inputText) {
  std::string *field = nullptr;
  int maxLen = 0;
  switch (activeField_) {
  case 0:
    field = &hostText_;
    maxLen = 64;
    break;
  case 1:
    field = &portText_;
    maxLen = 5;
    break;
  case 2:
    field = &loginText_;
    maxLen = 32;
    break;
  }
  if (!field)
    return true;
  if ((int)field->size() >= maxLen)
    return true;

  field->insert(cursorPos_, inputText);
  cursorPos_ += strlen(inputText);
  return true;
}

void DXClusterSetup::setConfig(const AppConfig &cfg) {
  hostText_ = cfg.dxClusterHost;
  portText_ = std::to_string(cfg.dxClusterPort);
  loginText_ = cfg.dxClusterLogin;
  useWSJTX_ = cfg.dxClusterUseWSJTX;
  cursorPos_ = hostText_.size();
}

AppConfig DXClusterSetup::updateConfig(AppConfig cfg) const {
  cfg.dxClusterHost = hostText_;
  cfg.dxClusterPort = std::atoi(portText_.c_str());
  if (cfg.dxClusterPort == 0)
    cfg.dxClusterPort = 7300;
  cfg.dxClusterLogin = loginText_;
  cfg.dxClusterUseWSJTX = useWSJTX_;
  return cfg;
}
