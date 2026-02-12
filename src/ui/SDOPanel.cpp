#include "SDOPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <mutex>

static std::mutex sdoMutex;
static std::string sdoPendingData;
static bool sdoDataReady = false;

SDOPanel::SDOPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   TextureManager &texMgr, SDOProvider &provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      provider_(provider) {
  tempId_ = currentId_;
}

void SDOPanel::update() {
  uint32_t now = SDL_GetTicks();

  // Hourly fetch or on ID change
  if (now - lastFetch_ > 60 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = now;
    provider_.fetch(currentId_, [](const std::string &data) {
      std::lock_guard<std::mutex> lock(sdoMutex);
      sdoPendingData = data;
      sdoDataReady = true;
    });
  }

  // Handle rotation (every 30 seconds if enabled)
  if (rotating_ && (now - lastRotate_ > 30000 || lastRotate_ == 0)) {
    lastRotate_ = now;
    // Advance to next wavelength
    int idx = 0;
    for (int i = 0; i < 7; ++i) {
      if (wavelengths_[i].id == currentId_) {
        idx = (i + 1) % 7;
        break;
      }
    }
    currentId_ = wavelengths_[idx].id;
    // Trigger immediate fetch
    lastFetch_ = 0;
  }
}

void SDOPanel::render(SDL_Renderer *renderer) {
  // 1. Check for new data
  {
    std::lock_guard<std::mutex> lock(sdoMutex);
    if (sdoDataReady) {
      texMgr_.loadFromMemory(renderer, "sdo_latest", sdoPendingData);
      sdoDataReady = false;
      sdoPendingData.clear();
      imageReady_ = true;
    }
  }

  ThemeColors themes = getThemeColors(theme_);

  // 2. Background and Border
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // 3. Draw Image
  SDL_Texture *tex = texMgr_.get("sdo_latest");
  if (tex && imageReady_) {
    int drawSz = std::min(width_, height_) - 4;
    SDL_Rect dst = {x_ + (width_ - drawSz) / 2, y_ + (height_ - drawSz) / 2,
                    drawSz, drawSz};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);

    renderOverlays(renderer, themes);
  } else {
    fontMgr_.drawText(renderer, "Loading SUN...", x_ + width_ / 2,
                      y_ + height_ / 2, themes.textDim, 12, false, true);
  }
}

void SDOPanel::renderModal(SDL_Renderer *renderer) {
  if (menuVisible_) {
    ThemeColors themes = getThemeColors(theme_);
    renderMenu(renderer, themes);
  }
}

void SDOPanel::renderOverlays(SDL_Renderer *renderer,
                              const ThemeColors &themes) {
  double az = 0, el = 0;
  auto now = std::chrono::system_clock::now();
  auto sunPos = Astronomy::sunPosition(now);
  Astronomy::calculateAzEl({obsLat_, obsLon_}, sunPos, az, el);

  char buf[32];
  SDL_Color HUD = {255, 165, 0, 255}; // Orange HUD

  // Az: NN
  std::snprintf(buf, sizeof(buf), "Az:%.0f", az);
  fontMgr_.drawText(renderer, buf, x_ + 4, y_ + 4, HUD, overlayFontSize_);

  // El: NN
  std::snprintf(buf, sizeof(buf), "El:%.0f", el);
  int elW = 0;
  TTF_SizeText(fontMgr_.getFont(overlayFontSize_), buf, &elW, nullptr);
  fontMgr_.drawText(renderer, buf, x_ + width_ - elW - 4, y_ + 4, HUD,
                    overlayFontSize_);

  // R@HH:MM (Rise/Set)
  time_t t0 = std::chrono::system_clock::to_time_t(now);
  struct tm localTM;
  localtime_r(&t0, &localTM);
  int doy = localTM.tm_yday + 1;
  auto st = Astronomy::calculateSunTimes(obsLat_, obsLon_, doy);

  if (st.hasRise) {
    int rh = (int)st.sunrise;
    int rm = (int)((st.sunrise - rh) * 60);
    std::snprintf(buf, sizeof(buf), "R@%02d:%02d", rh, rm);
    fontMgr_.drawText(renderer, buf, x_ + 4,
                      y_ + height_ - overlayFontSize_ - 4, HUD,
                      overlayFontSize_);
  }

  // Wavelength Name
  std::string wlName = "Unknown";
  for (const auto &w : wavelengths_) {
    if (w.id == currentId_) {
      wlName = w.name;
      break;
    }
  }
  std::snprintf(buf, sizeof(buf), "%s", wlName.c_str());
  int vW = 0;
  TTF_SizeText(fontMgr_.getFont(overlayFontSize_), buf, &vW, nullptr);
  fontMgr_.drawText(renderer, buf, x_ + width_ - vW - 4,
                    y_ + height_ - overlayFontSize_ - 4, HUD, overlayFontSize_);
}

void SDOPanel::renderMenu(SDL_Renderer *renderer, const ThemeColors &themes) {
  // Menu background (Glass style)
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 245);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &menuRect_);

  // Icon and Title
  SDL_Rect iconRect = {menuRect_.x + 10, menuRect_.y + 5, 24, 24};
  SDL_Color gearColor = {140, 140, 140, 255};
  SDL_Color bgColor = {20, 20, 20, 255};
  RenderUtils::drawGear(renderer, iconRect.x + iconRect.w / 2.0f,
                        iconRect.y + iconRect.h / 2.0f, iconRect.w / 2.0f,
                        gearColor, bgColor);
  fontMgr_.drawText(renderer, "SDO Wavelength", menuRect_.x + menuRect_.w / 2,
                    menuRect_.y + 5, themes.textDim, 14, false, true);

  // Radio Buttons (Highlight instead of boxes)
  for (int i = 0; i < 7; ++i) {
    bool selected = (tempId_ == wavelengths_[i].id && !tempRotating_);
    SDL_Rect r = radioRects_[i];

    if (selected) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 80);
      SDL_RenderFillRect(renderer, &r);
    }

    fontMgr_.drawText(renderer, wavelengths_[i].name, r.x + 10, r.y + 4,
                      selected ? themes.text : themes.textDim, 16);
  }

  // Rotate Toggle
  SDL_Rect rotIdx = rotateRect_;
  if (tempRotating_) {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 80);
    SDL_RenderFillRect(renderer, &rotIdx);
  }
  fontMgr_.drawText(renderer, "Auto-Rotate", rotIdx.x + 10, rotIdx.y + 4,
                    tempRotating_ ? themes.text : themes.textDim, 16);

  // Checkboxes (Simplified)
  auto drawToggle = [&](SDL_Rect r, bool val, const char *lbl) {
    if (val) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 80);
      SDL_RenderFillRect(renderer, &r);
    }
    fontMgr_.drawText(renderer, lbl, r.x + 10, r.y + 4,
                      val ? themes.text : themes.textDim, 16);
  };

  drawToggle(graylineRect_, tempGrayline_, "Grayline Tool");
  drawToggle(movieRect_, tempMovie_, "Show Movie");

  // OK / Cancel
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(renderer, &okRect_);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150);
  SDL_RenderDrawRect(renderer, &okRect_);
  SDL_RenderDrawRect(renderer, &cancelRect_);

  fontMgr_.drawText(renderer, "Ok", okRect_.x + okRect_.w / 2,
                    okRect_.y + okRect_.h / 2, themes.text, 18, false, true);
  fontMgr_.drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                    cancelRect_.y + cancelRect_.h / 2, themes.text, 18, false,
                    true);
}

void SDOPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (cat) {
    if (w < 120) {
      // Narrow fidelity slot
      menuFontSize_ = cat->ptSize(FontStyle::Micro);
      overlayFontSize_ = cat->ptSize(FontStyle::Micro);
    } else {
      menuFontSize_ = 16;
      overlayFontSize_ = 14;
    }
  }
  recalcMenuLayout();
}

void SDOPanel::recalcMenuLayout() {
  // Global centered popup (relative to 800x480 space)
  int mW = 280;
  int mH = 400;
  menuRect_ = {(800 - mW) / 2, (480 - mH) / 2, mW, mH};

  int curY = menuRect_.y + 25;
  itemH_ = 28;

  radioRects_.clear();
  radioRects_.reserve(7);
  for (int i = 0; i < 7; ++i) {
    radioRects_.push_back({menuRect_.x + 10, curY, mW - 20, itemH_});
    curY += itemH_;
  }

  rotateRect_ = {menuRect_.x + 10, curY, mW - 20, itemH_};
  curY += itemH_ + 5;

  graylineRect_ = {menuRect_.x + 10, curY, mW - 20, itemH_};
  curY += itemH_;
  movieRect_ = {menuRect_.x + 10, curY, mW - 20, itemH_};

  int btnW = 80;
  int btnH = 35;
  okRect_ = {menuRect_.x + 30, menuRect_.y + mH - 45, btnW, btnH};
  cancelRect_ = {menuRect_.x + mW - 30 - btnW, menuRect_.y + mH - 45, btnW,
                 btnH};
}

bool SDOPanel::onMouseUp(int mx, int my, Uint16 mod) {
  if (menuVisible_) {
    // 1. Check buttons FIRST to avoid overlap issues
    if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y &&
        my < okRect_.y + okRect_.h) {
      currentId_ = tempId_;
      rotating_ = tempRotating_;
      menuVisible_ = false;
      lastFetch_ = 0; // Trigger reload
      return true;
    }

    if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
        my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
      menuVisible_ = false;
      return true;
    }

    // 2. Handle menu items
    for (int i = 0; i < 7; ++i) {
      if (mx >= radioRects_[i].x && mx < radioRects_[i].x + radioRects_[i].w &&
          my >= radioRects_[i].y && my < radioRects_[i].y + radioRects_[i].h) {
        tempId_ = wavelengths_[i].id;
        tempRotating_ = false;
        return true;
      }
    }

    if (mx >= rotateRect_.x && mx < rotateRect_.x + rotateRect_.w &&
        my >= rotateRect_.y && my < rotateRect_.y + rotateRect_.h) {
      tempRotating_ = true;
      return true;
    }

    if (mx >= graylineRect_.x && mx < graylineRect_.x + graylineRect_.w &&
        my >= graylineRect_.y && my < graylineRect_.y + graylineRect_.h) {
      tempGrayline_ = !tempGrayline_;
      return true;
    }

    if (mx >= movieRect_.x && mx < movieRect_.x + movieRect_.w &&
        my >= movieRect_.y && my < movieRect_.y + movieRect_.h) {
      tempMovie_ = !tempMovie_;
      return true;
    }

    return true; // Click eaten by modal
  }

  // Open menu on click
  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    menuVisible_ = true;
    tempId_ = currentId_;
    tempRotating_ = rotating_;
    recalcMenuLayout();
    return true;
  }

  return false;
}

bool SDOPanel::onKeyDown(SDL_Keycode key, Uint16 /*mod*/) {
  if (menuVisible_) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      currentId_ = tempId_;
      rotating_ = tempRotating_;
      menuVisible_ = false;
      lastFetch_ = 0;
      return true;
    }
    if (key == SDLK_ESCAPE) {
      menuVisible_ = false;
      return true;
    }
    return true; // Eat all keys when modal
  }
  return false;
}
