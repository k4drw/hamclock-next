#include "SetupScreen.h"
#include "../core/Astronomy.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

SetupScreen::SetupScreen(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
  recalcLayout();
}

void SetupScreen::recalcLayout() {
  int h = height_;
  titleSize_ = std::clamp(static_cast<int>(h * 0.06f), 18, 48);
  labelSize_ = std::clamp(static_cast<int>(h * 0.035f), 12, 24);
  fieldSize_ = std::clamp(static_cast<int>(h * 0.045f), 14, 32);
  hintSize_ = std::clamp(static_cast<int>(h * 0.028f), 10, 18);
}

void SetupScreen::autoPopulateLatLon() {
  // Force canonical casing for immediate feedback
  for (size_t i = 0; i < gridText_.size(); ++i) {
    if (i < 2) {
      if (gridText_[i] >= 'a' && gridText_[i] <= 'z')
        gridText_[i] -= 32;
    } else if (i >= 4) {
      if (gridText_[i] >= 'A' && gridText_[i] <= 'Z')
        gridText_[i] += 32;
    }
  }

  if (gridText_.size() >= 4) {
    gridValid_ = Astronomy::gridToLatLon(gridText_, gridLat_, gridLon_);
  } else {
    gridValid_ = false;
  }

  if (gridValid_ && !latLonManual_) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", gridLat_);
    latText_ = buf;
    std::snprintf(buf, sizeof(buf), "%.4f", gridLon_);
    lonText_ = buf;
  }
}

void SetupScreen::update() {
  autoPopulateLatLon();

  // Check mismatch: if lat/lon were manually edited, see if they fall outside
  // the grid
  mismatchWarning_ = false;
  if (latLonManual_ && gridValid_ && !latText_.empty() && !lonText_.empty()) {
    double manLat = std::atof(latText_.c_str());
    double manLon = std::atof(lonText_.c_str());
    // Grid subsquare covers ~2.5° lon x ~1.25° lat (for 4-char) or smaller for
    // 6-char
    double tolLat = (gridText_.size() >= 6) ? 0.5 : 1.0;
    double tolLon = (gridText_.size() >= 6) ? 1.0 : 2.0;
    if (std::fabs(manLat - gridLat_) > tolLat ||
        std::fabs(manLon - gridLon_) > tolLon) {
      mismatchWarning_ = true;
    }
  }
}

// Helper: render a single field (background, border, text, cursor)
static void renderField(SDL_Renderer *renderer, FontManager &fontMgr,
                        const std::string &text, const std::string &placeholder,
                        int fieldX, int &y, int fieldW, int fieldH,
                        int fieldSize, int textPad, bool active, bool valid,
                        int cursorPos, SDL_Color activeBorder,
                        SDL_Color inactiveBorder, SDL_Color validColor,
                        SDL_Color textColor, SDL_Color placeholderColor) {
  SDL_Color border = active ? activeBorder : inactiveBorder;

  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_Rect rect = {fieldX, y, fieldW, fieldH};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (!text.empty()) {
    SDL_Color color = valid ? validColor : textColor;
    fontMgr.drawText(renderer, text, fieldX + textPad, y + textPad, color,
                     fieldSize);
  } else if (!active) {
    fontMgr.drawText(renderer, placeholder, fieldX + textPad, y + textPad,
                     placeholderColor, fieldSize);
  }

  // Blinking cursor
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

void SetupScreen::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Dark background
  SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  int cx = x_ + width_ / 2;
  int pad = std::max(16, width_ / 24);
  int fieldW = std::min(400, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;
  int textPad = 7;
  int halfFieldW = (fieldW - pad) / 2;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color green = {0, 200, 0, 255};
  SDL_Color red = {255, 80, 80, 255};

  int y = y_ + height_ / 8;

  // --- Title ---
  SDL_Color cyan = {0, 200, 255, 255};
  {
    TTF_Font *font = fontMgr_.getFont(titleSize_);
    if (font) {
      int tw = 0, th = 0;
      TTF_SizeText(font, "Welcome to HamClock-Next", &tw, &th);
      fontMgr_.drawText(renderer, "Welcome to HamClock-Next", cx - tw / 2, y,
                        cyan, titleSize_, true);
      y += th + pad;
    }
  }

  // --- Callsign ---
  fontMgr_.drawText(renderer, "Callsign:", fieldX, y, white, labelSize_, true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, callsignText_, "e.g. K4DRW", fieldX, y,
              fieldW, fieldH, fieldSize_, textPad, activeField_ == 0,
              !callsignText_.empty(), cursorPos_, orange, gray, white, white,
              gray);
  y += pad;

  // --- Grid ---
  fontMgr_.drawText(renderer, "Grid Square:", fieldX, y, white, labelSize_,
                    true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, gridText_, "e.g. EL87qr", fieldX, y, fieldW,
              fieldH, fieldSize_, textPad, activeField_ == 1, gridValid_,
              cursorPos_, orange, gray, green, white, gray);
  y += pad;

  // --- Lat / Lon (side by side) ---
  fontMgr_.drawText(renderer, "Latitude:", fieldX, y, white, labelSize_, true);
  fontMgr_.drawText(renderer, "Longitude:", fieldX + halfFieldW + pad, y, white,
                    labelSize_, true);
  y += labelSize_ + 4;

  int latY = y;
  renderField(renderer, fontMgr_, latText_, "e.g. 27.7600", fieldX, latY,
              halfFieldW, fieldH, fieldSize_, textPad, activeField_ == 2,
              !latText_.empty(), cursorPos_, orange, gray, white, white, gray);

  int lonY = y;
  renderField(renderer, fontMgr_, lonText_, "e.g. -82.6400",
              fieldX + halfFieldW + pad, lonY, halfFieldW, fieldH, fieldSize_,
              textPad, activeField_ == 3, !lonText_.empty(), cursorPos_, orange,
              gray, white, white, gray);

  y = std::max(latY, lonY) + pad / 2;

  // Auto-calc hint or mismatch warning
  if (mismatchWarning_) {
    fontMgr_.drawText(renderer, "Warning: Lat/Lon outside grid square", fieldX,
                      y, red, hintSize_);
  } else if (gridValid_ && !latLonManual_) {
    fontMgr_.drawText(renderer, "Auto-calculated from grid", fieldX, y, gray,
                      hintSize_);
  }
  y += hintSize_ + pad;

  // --- Hints ---
  fontMgr_.drawText(renderer, "Tab = next field    Enter = save & start",
                    fieldX, y, gray, hintSize_);
}

void SetupScreen::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool SetupScreen::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  (void)mx;
  (void)my;
  return true;
}

bool SetupScreen::onKeyDown(SDL_Keycode key, Uint16 /*mod*/) {
  // Determine active text and max length
  std::string *text = nullptr;
  int maxLen = 0;
  switch (activeField_) {
  case 0:
    text = &callsignText_;
    maxLen = 12;
    break;
  case 1:
    text = &gridText_;
    maxLen = 6;
    break;
  case 2:
    text = &latText_;
    maxLen = 12;
    break;
  case 3:
    text = &lonText_;
    maxLen = 12;
    break;
  }
  if (!text)
    return true;

  switch (key) {
  case SDLK_TAB:
    activeField_ = (activeField_ + 1) % kNumFields;
    switch (activeField_) {
    case 0:
      cursorPos_ = static_cast<int>(callsignText_.size());
      break;
    case 1:
      cursorPos_ = static_cast<int>(gridText_.size());
      break;
    case 2:
      cursorPos_ = static_cast<int>(latText_.size());
      break;
    case 3:
      cursorPos_ = static_cast<int>(lonText_.size());
      break;
    }
    return true;

  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    if (!callsignText_.empty() && gridValid_ && !latText_.empty() &&
        !lonText_.empty()) {
      complete_ = true;
    }
    return true;

  case SDLK_BACKSPACE:
    if (cursorPos_ > 0) {
      text->erase(cursorPos_ - 1, 1);
      --cursorPos_;
      if (activeField_ == 2 || activeField_ == 3)
        latLonManual_ = true;
    }
    return true;

  case SDLK_DELETE:
    if (cursorPos_ < static_cast<int>(text->size())) {
      text->erase(cursorPos_, 1);
      if (activeField_ == 2 || activeField_ == 3)
        latLonManual_ = true;
    }
    return true;

  case SDLK_LEFT:
    if (cursorPos_ > 0)
      --cursorPos_;
    return true;

  case SDLK_RIGHT:
    if (cursorPos_ < static_cast<int>(text->size()))
      ++cursorPos_;
    return true;

  case SDLK_HOME:
    cursorPos_ = 0;
    return true;

  case SDLK_END:
    cursorPos_ = static_cast<int>(text->size());
    return true;

  default:
    return true;
  }
  (void)maxLen;
}

bool SetupScreen::onTextInput(const char *inputText) {
  std::string *field = nullptr;
  int maxLen = 0;
  switch (activeField_) {
  case 0:
    field = &callsignText_;
    maxLen = 12;
    break;
  case 1:
    field = &gridText_;
    maxLen = 6;
    break;
  case 2:
    field = &latText_;
    maxLen = 12;
    break;
  case 3:
    field = &lonText_;
    maxLen = 12;
    break;
  }
  if (!field)
    return true;

  if (static_cast<int>(field->size()) >= maxLen)
    return true;

  // For lat/lon fields, only allow digits, minus, and dot
  if (activeField_ == 2 || activeField_ == 3) {
    for (const char *p = inputText; *p; ++p) {
      if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
        continue;
      return true; // reject non-numeric input
    }
    latLonManual_ = true;
  }

  field->insert(cursorPos_, inputText);
  cursorPos_ += static_cast<int>(std::strlen(inputText));

  if (static_cast<int>(field->size()) > maxLen) {
    field->resize(maxLen);
    if (cursorPos_ > maxLen)
      cursorPos_ = maxLen;
  }

  // If grid changed, reset manual flag so auto-populate kicks in
  if (activeField_ == 1) {
    latLonManual_ = false;
  }

  return true;
}

void SetupScreen::setConfig(const AppConfig &cfg) {
  callsignText_ = cfg.callsign;
  gridText_ = cfg.grid;

  // Populate lat/lon from config values
  if (cfg.lat != 0.0 || cfg.lon != 0.0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lat);
    latText_ = buf;
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lon);
    lonText_ = buf;
  }

  cursorPos_ = static_cast<int>(callsignText_.size());
}

AppConfig SetupScreen::getConfig() const {
  AppConfig cfg;
  cfg.callsign = callsignText_;
  cfg.grid = gridText_;
  cfg.lat = std::atof(latText_.c_str());
  cfg.lon = std::atof(lonText_.c_str());
  return cfg;
}
