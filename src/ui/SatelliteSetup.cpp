#include "SatelliteSetup.h"
#include "../core/Constants.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cctype>

namespace HamClock {

SatelliteSetup::SatelliteSetup(int x, int y, int w, int h, FontManager &fontMgr,
                               SatelliteManager &satMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), satMgr_(satMgr) {
  calculateLayout();
}

void SatelliteSetup::calculateLayout() {
  // Use logical dimensions for full-screen modal if requested, otherwise stay
  // within assigned bounds.
  int effectiveW = (width_ > 200) ? width_ : LOGICAL_WIDTH;
  int effectiveH = (height_ > 200) ? height_ : LOGICAL_HEIGHT;
  int startX = (width_ > 200) ? x_ : 0;
  int startY = (height_ > 200) ? y_ : 0;

  // Center a dialog box
  int modalW = std::clamp(static_cast<int>(effectiveW * 0.7f), 300, 500);
  int modalH = std::clamp(static_cast<int>(effectiveH * 0.8f), 250, 400);
  int modalX = startX + (effectiveW - modalW) / 2;
  int modalY = startY + (effectiveH - modalH) / 2;

  dialogRect_ = {modalX, modalY, modalW, modalH};

  int pad = 15;
  int inputH = 28;

  rectSccInput_ = {modalX + pad, modalY + 60, modalW - 2 * pad, inputH};
  rectTleInput_ = {modalX + pad, modalY + 115, modalW - 2 * pad, 80};
  rectList_ = {modalX + pad, modalY + 225, modalW - 2 * pad, modalH - 280};

  int btnW = 100;
  int btnH = 32;
  int bx = modalX + modalW / 2;
  int by = modalY + modalH - btnH - 15;
  rectCancel_ = {bx - btnW - 10, by, btnW, btnH};
  rectOk_ = {bx + 10, by, btnW, btnH};
}

void SatelliteSetup::setActive(bool active) {
  active_ = active;
  if (active_) {
    inputMode_ = InputMode::None;
    sccInput_.clear();
    sccInput_.setActive(false);
    tleInput_.clear();
    SDL_StartTextInput();
  } else {
    SDL_StopTextInput();
  }
}

void SatelliteSetup::update() {}

void SatelliteSetup::render(SDL_Renderer *renderer) {
  if (!active_)
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background box
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &dialogRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &dialogRect_);

  // Title
  cat->drawText(renderer, "Add Custom Satellite",
                dialogRect_.x + dialogRect_.w / 2, dialogRect_.y + 25,
                themes.accent, FontStyle::FastBold, true);

  // SCC Input
  cat->drawText(renderer, "NORAD ID (SCC):", rectSccInput_.x,
                rectSccInput_.y - 18, themes.text, FontStyle::Fast);
  sccInput_.render(renderer, fontMgr_,
                   rectSccInput_.x, rectSccInput_.y,
                   rectSccInput_.w, rectSccInput_.h,
                   FontStyle::Fast, 8,
                   inputMode_ == InputMode::SCC, true,
                   themes.accent,
                   {themes.border.r, themes.border.g, themes.border.b, 100},
                   themes.text, themes.text, themes.textDim,
                   "Click to type ID...", &themes.rowStripe1);

  // TLE Input
  cat->drawText(renderer, "TLE (3 lines):", rectTleInput_.x,
                rectTleInput_.y - 18, themes.text, FontStyle::Fast);
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rectTleInput_);

  if (inputMode_ == InputMode::TLE) {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 255);
  } else {
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 100);
  }
  SDL_RenderDrawRect(renderer, &rectTleInput_);

  std::string tleDisp = tleInput_.empty() ? "Paste TLE here..." : "Data Loaded";
  SDL_Color tleColor = tleInput_.empty() ? themes.textDim : themes.text;
  cat->drawText(renderer, tleDisp.c_str(), rectTleInput_.x + 8,
                rectTleInput_.y + 5, tleColor, FontStyle::Fast);

  // Buttons
  auto drawBtn = [&](const SDL_Rect &r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 150);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2,
                  themes.bg, FontStyle::UIBold, true, false, true);
  };

  drawBtn(rectCancel_, "Cancel", themes.danger);
  drawBtn(rectOk_, "Done", themes.success);

  // Custom Satellites List
  int listY = rectList_.y;
  cat->drawText(renderer, "Current Custom Satellites:", rectList_.x, listY - 18,
                themes.text, FontStyle::SmallRegular);

  SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                         themes.rowStripe2.b, 255);
  SDL_RenderFillRect(renderer, &rectList_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &rectList_);

  const auto &customSats = satMgr_.getCustomSatellites();
  int yOff = 5;
  for (const auto &sat : customSats) {
    if (yOff + 20 > rectList_.h)
      break;

    cat->drawText(renderer, sat.name.c_str(), rectList_.x + 5,
                  rectList_.y + yOff, themes.text, FontStyle::Fast);

    // Delete button
    SDL_Rect delBtn = {rectList_.x + rectList_.w - 60, rectList_.y + yOff - 2,
                       55, 18};
    SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                           themes.danger.b, 100);
    SDL_RenderFillRect(renderer, &delBtn);
    cat->drawText(renderer, "Delete", delBtn.x + delBtn.w / 2,
                  delBtn.y + delBtn.h / 2, themes.text, FontStyle::MicroBold,
                  true, false, true);

    yOff += 22;
  }
  if (customSats.empty()) {
    cat->drawText(renderer, "None added yet.", rectList_.x + 5, rectList_.y + 5,
                  themes.textDim, FontStyle::Fast);
  }
}

bool SatelliteSetup::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (!active_)
    return false;

  auto pointInRect = [](int x, int y, const SDL_Rect &r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
  };

  if (pointInRect(mx, my, rectSccInput_)) {
    int oldMode = (int)inputMode_;
    inputMode_ = InputMode::SCC;
    sccInput_.setActive(true);
    if (oldMode == (int)InputMode::SCC || clicks == 2)
      sccInput_.onMouseDown(mx, my, clicks, fontMgr_, rectSccInput_.x,
                            rectSccInput_.y, rectSccInput_.w, rectSccInput_.h,
                            FontStyle::SmallRegular, 7);
    else
      sccInput_.setCursorToEnd();
    return true;
  }
  if (pointInRect(mx, my, rectTleInput_)) {
    inputMode_ = InputMode::TLE;
    sccInput_.setActive(false);
    return true;
  }

  if (pointInRect(mx, my, rectList_)) {
    const auto &customSats = satMgr_.getCustomSatellites();
    int yOff = 5;
    for (const auto &sat : customSats) {
      if (yOff + 20 > rectList_.h)
        break;
      SDL_Rect delRect = {rectList_.x + rectList_.w - 60, rectList_.y + yOff - 2,
                          55, 18};
      if (pointInRect(mx, my, delRect)) {
        if (sat.noradId > 0 &&
            std::find(
                ConfigManager::instance()
                    .getConfig()
                    .customSatelliteSCCs.begin(),
                ConfigManager::instance().getConfig().customSatelliteSCCs.end(),
                sat.noradId) != ConfigManager::instance()
                                    .getConfig()
                                    .customSatelliteSCCs.end()) {
          satMgr_.removeCustomSCC(sat.noradId);
        } else {
          satMgr_.removeCustomTLE(sat.name);
        }
        return true;
      }
      yOff += 22;
    }
  }

  // If clicking outside dialog but within widget bounds, close?
  // No, let onMouseUp handle it or just keep it modal.
  return true;
}

bool SatelliteSetup::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!active_)
    return false;

  auto pointInRect = [](int x, int y, const SDL_Rect &r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
  };

  if (pointInRect(mx, my, rectOk_)) {
    if (!sccInput_.getValue().empty())
      addSatelliteBySCC();
    if (!tleInput_.empty())
      addSatelliteByTLE();
    setActive(false);
    return true;
  }
  if (pointInRect(mx, my, rectCancel_)) {
    setActive(false);
    return true;
  }

  // Click outside dialog to dismiss?
  if (!pointInRect(mx, my, dialogRect_)) {
    // setActive(false);
    // return true;
  }

  return true;
}

bool SatelliteSetup::onTextInput(const char *text) {
  if (!active_)
    return false;

  if (inputMode_ == InputMode::SCC) {
    for (size_t i = 0; i < strlen(text); ++i) {
      if (isdigit(text[i])) {
        char buf[2] = {text[i], '\0'};
        sccInput_.onTextInput(buf);
      }
    }
    return true;
  }
  if (inputMode_ == InputMode::TLE) {
    tleInput_ += text;
    return true;
  }

  return false;
}

bool SatelliteSetup::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!active_)
    return false;

  if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      if (!sccInput_.getValue().empty())
        addSatelliteBySCC();
      if (!tleInput_.empty())
        addSatelliteByTLE();
    }
    setActive(false);
    return true;
  }

  if (inputMode_ == InputMode::SCC) {
    if (key == SDLK_v && (mod & KMOD_CTRL)) {
      char *cbText = SDL_GetClipboardText();
      if (cbText) {
        sccInput_.clear();
        for (size_t i = 0; i < strlen(cbText); ++i) {
          if (isdigit(cbText[i])) {
            char buf[2] = {cbText[i], '\0'};
            sccInput_.onTextInput(buf);
          }
        }
        SDL_free(cbText);
      }
      return true;
    }
    sccInput_.onKeyDown(key, mod);
    return true;
  }

  if (inputMode_ == InputMode::TLE) {
    if (key == SDLK_BACKSPACE && !tleInput_.empty())
      tleInput_.pop_back();
    else if (key == SDLK_v && (mod & KMOD_CTRL)) {
      char *cbText = SDL_GetClipboardText();
      if (cbText) {
        tleInput_ = cbText;
        SDL_free(cbText);
      }
    }
    return true;
  }

  return true;
}

void SatelliteSetup::addSatelliteBySCC() {
  int id = std::atoi(sccInput_.getValue().c_str());
  if (id > 0) {
    satMgr_.addCustomSCC(id);
  }
}

void SatelliteSetup::addSatelliteByTLE() {
  std::istringstream stream(tleInput_);
  std::string n, l1, l2;
  if (std::getline(stream, n) && std::getline(stream, l1) &&
      std::getline(stream, l2)) {
    SatelliteTLE tle;
    tle.name = n;
    tle.line1 = l1;
    tle.line2 = l2;
    if (tle.line1.size() >= 7) {
      tle.noradId = std::atoi(tle.line1.substr(2, 5).c_str());
    }
    satMgr_.addCustomTLE(tle);
  }
}

nlohmann::json SatelliteSetup::getDebugData() const {
  return {{"active", active_}, {"input_mode", static_cast<int>(inputMode_)}};
}

} // namespace HamClock
