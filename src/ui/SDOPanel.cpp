#include "SDOPanel.h"
#include "../core/Astronomy.h"
#include "../core/ConfigManager.h"
#include "../core/Constants.h"
#include "../core/Theme.h"
#include "../core/WorkerService.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <nlohmann/json.hpp>

SDOPanel::SDOPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   TextureManager &texMgr, SDOProvider &provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      provider_(provider) {
  const auto &config = ConfigManager::instance().getConfig();
  currentId_ = config.sdoWavelength;
  rotating_ = config.sdoRotating;
  showPfss_ = config.sdoPfss;
  movie_ = config.sdoShowMovie;

  tempId_ = currentId_;
  tempRotating_ = rotating_;
  tempPfss_ = showPfss_;
  tempMovie_ = movie_;

  recalcMenuLayout();
}

SDOPanel::~SDOPanel() {
  std::lock_guard<std::mutex> lock(*pendingMutex_);
  if (*pendingSurface_) {
    SDL_FreeSurface(*pendingSurface_);
    *pendingSurface_ = nullptr;
  }
}

void SDOPanel::update() {
  uint32_t now = SDL_GetTicks();
  // Hourly fetch or on ID change

  // Hourly fetch or on ID change
  if (now - lastFetch_ > 60 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = now;
    auto mtx = pendingMutex_;
    auto psurf = pendingSurface_;
    auto rdy = dataReady_;
    auto stm = pendingServerTime_;
    bool pfss = showPfss_;
    std::string cid = currentId_;

    provider_.fetch(cid, pfss,
                    [mtx, psurf, rdy, stm, cid](const std::string &data,
                                                std::time_t serverTime) {
                      WorkerService::getInstance().submitTask([mtx, psurf, rdy,
                                                               stm, data, cid,
                                                               serverTime]() {
                        // Decode and tint in background
                        SDL_Color tint = {255, 255, 255, 255};
                        if (cid == "HMIIC") {
                          tint = {255, 147, 41, 255};
                        }

                        SDL_Surface *surf = TextureManager::decodeToSurface(
                            reinterpret_cast<const unsigned char *>(
                                data.data()),
                            static_cast<unsigned int>(data.size()),
                            "sdo_latest", tint);

                        if (surf) {
                          std::lock_guard<std::mutex> lock(*mtx);
                          if (*psurf) {
                            SDL_FreeSurface(*psurf);
                          }
                          *psurf = surf;
                          *stm = serverTime;
                          *rdy = true;
                        }
                      });
                    });
  }

  // Handle rotation (every 30 seconds if enabled)
  if (rotating_ && (now - lastRotate_ > 30000 || lastRotate_ == 0)) {
    lastRotate_ = now;
    // Advance to next wavelength
    int idx = 0;
    int nWl = sizeof(wavelengths_) / sizeof(wavelengths_[0]);
    for (int i = 0; i < nWl; ++i) {
      if (wavelengths_[i].id == currentId_) {
        idx = (i + 1) % nWl;
        break;
      }
    }
    currentId_ = wavelengths_[idx].id;
    // Trigger immediate fetch
    lastFetch_ = 0;
  }
}

void SDOPanel::render(SDL_Renderer *renderer) {
  // 1. Check for new data from background decoder
  {
    std::lock_guard<std::mutex> lock(*pendingMutex_);
    if (*dataReady_) {
      if (*pendingSurface_) {
        texMgr_.loadFromSurface(renderer, "sdo_latest", *pendingSurface_);
        SDL_FreeSurface(*pendingSurface_);
        *pendingSurface_ = nullptr;
        imageServerTime_ = *pendingServerTime_;
        imageReady_ = true;
      }
      *dataReady_ = false;
    }
  }

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  // 3. Draw Image
  SDL_Texture *tex = texMgr_.get("sdo_latest");
  if (tex && imageReady_) {
    int titleH = height_ / 10;
    int drawSz = std::min(width_, height_ - titleH) - 6;
    SDL_Rect dst = {x_ + (width_ - drawSz) / 2,
                    y_ + titleH + (height_ - titleH - drawSz) / 2, drawSz,
                    drawSz};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);

    fontMgr_.catalog()->drawText(renderer, "SDO Solar", x_ + 10, y_ + 5,
                                 themes.accent, FontStyle::MicroBold);

    renderOverlays(renderer, themes);
  } else {
    fontMgr_.catalog()->drawText(renderer, "Loading SUN...", x_ + width_ / 2,
                                 y_ + height_ / 2, themes.textDim,
                                 FontStyle::Fast, true);
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
  auto *cat = fontMgr_.catalog();

  char buf[32];
  SDL_Color HUD = themes.accent; // Themed HUD

  int titleH = height_ / 10;
  // Az: NN
  std::snprintf(buf, sizeof(buf), "Az:%.0f", az);
  cat->drawText(renderer, buf, x_ + 4, y_ + titleH + 4, HUD, FontStyle::Micro);

  // El: NN
  std::snprintf(buf, sizeof(buf), "El:%.0f", el);
  int elW = fontMgr_.getLogicalWidth(buf, cat->ptSize(FontStyle::Micro));
  cat->drawText(renderer, buf, x_ + width_ - elW - 4, y_ + titleH + 4, HUD,
                FontStyle::Micro);

  // R@HH:MM (Rise/Set)
  time_t t0 = std::chrono::system_clock::to_time_t(now);
  struct tm localTM;
  Astronomy::portable_localtime(&t0, &localTM);
  int doy = localTM.tm_yday + 1;
  auto st = Astronomy::calculateSunTimes(obsLat_, obsLon_, doy);

  if (st.hasRise) {
    int rh = (int)st.sunrise;
    int rm = (int)((st.sunrise - rh) * 60);
    std::snprintf(buf, sizeof(buf), "R@%02d:%02d", rh, rm);
    cat->drawText(renderer, buf, x_ + 4, y_ + height_ - overlayFontSize_ - 4,
                  HUD, FontStyle::Micro);
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
  int vW = fontMgr_.getLogicalWidth(buf, cat->ptSize(FontStyle::Micro));
  cat->drawText(renderer, buf, x_ + width_ - vW - 4,
                y_ + height_ - overlayFontSize_ - 4, HUD, FontStyle::Micro);

  // Stale check (AIA: 1h, HMI: 24h as they update less frequently)
  if (imageServerTime_ > 0) {
    std::time_t nowT = std::time(nullptr);
    std::time_t threshold = 86400;
    if (nowT - imageServerTime_ > threshold) {
      // Draw "NOT CURRENT" in red
      SDL_Color staleColor = themes.danger;
      cat->drawText(renderer, "NOT CURRENT", x_ + width_ / 2, y_ + height_ / 2,
                    staleColor, FontStyle::SmallBold, true);
    }
  }
}

void SDOPanel::renderMenu(SDL_Renderer *renderer, const ThemeColors &themes) {
  // Menu background (Glass style)
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 245);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &menuRect_);

  // Icon and Title
  SDL_Rect iconRect = {menuRect_.x + 10, menuRect_.y + 8, 12, 12};
  SDL_Color gearColor = themes.textDim;
  SDL_Color bgColor = themes.bg;
  RenderUtils::drawGear(renderer, iconRect.x + iconRect.w / 2.0f,
                        iconRect.y + iconRect.h / 2.0f, iconRect.w / 2.0f,
                        gearColor, bgColor);
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "SDO Wavelength", menuRect_.x + menuRect_.w / 2,
                menuRect_.y + 5, themes.textDim, FontStyle::SmallRegular, true);

  // Radio Buttons (Highlight instead of boxes)
  int nWl = sizeof(wavelengths_) / sizeof(wavelengths_[0]);
  for (int i = 0; i < nWl; ++i) {
    bool selected = (tempId_ == wavelengths_[i].id && !tempRotating_);
    SDL_Rect r = radioRects_[i];

    if (selected) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 80);
      SDL_RenderFillRect(renderer, &r);
    }

    cat->drawText(renderer, wavelengths_[i].name, r.x + 10, r.y + 4,
                  selected ? themes.text : themes.textDim, FontStyle::UI);
  }

  // Rotate Toggle
  SDL_Rect rotIdx = rotateRect_;
  if (tempRotating_) {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 80);
    SDL_RenderFillRect(renderer, &rotIdx);
  }
  cat->drawText(renderer, "Auto-Rotate", rotIdx.x + 10, rotIdx.y + 4,
                tempRotating_ ? themes.text : themes.textDim, FontStyle::UI);

  // PFSS Magnetic Lines
  SDL_Rect pfssR = graylineRect_;
  if (tempPfss_) {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 80);
    SDL_RenderFillRect(renderer, &pfssR);
  }
  cat->drawText(renderer, "Magnetic Lines (PFSS)", pfssR.x + 10, pfssR.y + 4,
                tempPfss_ ? themes.text : themes.textDim, FontStyle::UI);

  // Done / Cancel
  auto drawBtn = [&](const SDL_Rect &r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 150);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, themes.bg,
                  FontStyle::SmallBold, true, false, true);
  };

  drawBtn(okRect_, "Done", themes.success);
  drawBtn(cancelRect_, "Cancel", themes.danger);
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
  int mH = 430;
  menuRect_ = {(HamClock::LOGICAL_WIDTH - mW) / 2,
               (HamClock::LOGICAL_HEIGHT - mH) / 2, mW, mH};

  int curY = menuRect_.y + 25;
  itemH_ = 25;

  int nWl = sizeof(wavelengths_) / sizeof(wavelengths_[0]);
  radioRects_.clear();
  radioRects_.reserve(nWl);
  for (int i = 0; i < nWl; ++i) {
    radioRects_.push_back({menuRect_.x + 10, curY, mW - 20, itemH_});
    curY += itemH_;
  }

  rotateRect_ = {menuRect_.x + 10, curY, mW - 20, itemH_};
  curY += itemH_ + 5;

  graylineRect_ = {menuRect_.x + 10, curY, mW - 20, itemH_};

  int btnW = 100;
  int btnH = 34;
  int spacing = 20;
  int totalBtnsW = 2 * btnW + spacing;
  int startX = menuRect_.x + (mW - totalBtnsW) / 2;

  okRect_ = {startX, menuRect_.y + mH - 45, btnW, btnH};
  cancelRect_ = {startX + btnW + spacing, menuRect_.y + mH - 45, btnW, btnH};
}

bool SDOPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (menuVisible_) {
    // 1. Check buttons FIRST to avoid overlap issues
    if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y &&
        my < okRect_.y + okRect_.h) {
      currentId_ = tempId_;
      rotating_ = tempRotating_;
      saveSettings();
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
    int nWl = sizeof(wavelengths_) / sizeof(wavelengths_[0]);
    for (int i = 0; i < nWl; ++i) {
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
      tempPfss_ = !tempPfss_;
      return true;
    }

    return true; // Click eaten by modal
  }

  // Open menu on click (ignore top 10% reserved for widget selector)
  if (mx >= x_ && mx < x_ + width_ && my >= y_ + height_ / 10 &&
      my < y_ + height_) {
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
      saveSettings();
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

void SDOPanel::saveSettings() {
  showPfss_ = tempPfss_;
  movie_ = tempMovie_;
  auto &config = ConfigManager::instance().getConfig();
  config.sdoWavelength = currentId_;
  config.sdoRotating = rotating_;
  config.sdoPfss = showPfss_;
  config.sdoShowMovie = movie_;
  ConfigManager::instance().save(config);
}

std::vector<std::string> SDOPanel::getActions() const {
  return {"cycle_wavelength"};
}

SDL_Rect SDOPanel::getActionRect(const std::string &action) const {
  (void)action; // All actions use the full widget rect
  return {x_, y_, width_, height_};
}

nlohmann::json SDOPanel::getDebugData() const {
  nlohmann::json data;

  // Find the current wavelength name
  const char *currentName = "Unknown";
  for (const auto &w : wavelengths_) {
    if (currentId_ == w.id) {
      currentName = w.name;
      break;
    }
  }

  data["current_wavelength"] = currentName;
  data["current_id"] = currentId_;
  data["rotating"] = rotating_;
  data["image_ready"] = imageReady_;
  data["image_server_time"] = imageServerTime_;
  data["image_age_seconds"] =
      (imageServerTime_ > 0) ? (std::time(nullptr) - imageServerTime_) : -1;

  // Available wavelengths
  nlohmann::json wavelengths = nlohmann::json::array();
  for (const auto &w : wavelengths_) {
    nlohmann::json wl;
    wl["name"] = w.name;
    wl["id"] = w.id;
    wl["active"] = (currentId_ == w.id);
    wavelengths.push_back(wl);
  }
  data["available_wavelengths"] = wavelengths;

  return data;
}
