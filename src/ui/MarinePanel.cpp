#include "MarinePanel.h"
#include "../services/MarineProvider.h"
#include "WidgetRegistry.h"
#include "../core/ConfigManager.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

MarinePanel::MarinePanel(int x, int y, int w, int h, FontManager &fontMgr,
                         std::shared_ptr<MarineStore> store,
                         MarineProvider *provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      provider_(provider) {
  stationInput_.setMaxLength(8);
  buoyInput_.setMaxLength(8);
}

void MarinePanel::onLookupReady(const MarineLookupResult &res) {
  isSearching_ = false;
  if (!res.closestTideId.empty()) {
    stationInput_.setValue(res.closestTideId);
    currentData_.tideStationName.clear();
  }
  if (!res.closestBuoyId.empty()) {
    buoyInput_.setValue(res.closestBuoyId);
  }
}



void MarinePanel::update() { currentData_ = store_->get(); }

void MarinePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  int titleH = 16;
  int pad = 6;
  int centerX = x_ + width_ / 2;
  int curY = y_ + titleH + 2;
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Marine", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  // Draw location (Buoy ID or Tide Station Name) - Now below title per user
  std::string locText = currentData_.tideStationName;
  if (locText.empty() && !currentData_.tideStationId.empty()) {
    locText = "Tide ID: " + currentData_.tideStationId;
  }
  if (!locText.empty()) {
    if (locText.size() > 24) locText = locText.substr(0, 22) + "..";
    cat->drawText(renderer, locText, x_ + 10, y_ + 16, themes.textDim,
                  FontStyle::Tiny, false, false);
    curY += 10;
  }

  if (!currentData_.tidesValid && !currentData_.buoyValid) {

    cat->drawText(renderer, "Click to configure", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true);
    return;
  }

  // Tide section
  if (currentData_.tidesValid && !currentData_.tides.empty()) {
    cat->drawText(renderer, "TIDES", x_ + pad, curY, themes.accent,
                  FontStyle::TinyBold);
    curY += subFontSize_ + 2;

    for (size_t i = 0; i < currentData_.tides.size() && i < 4; ++i) {
      const auto &t = currentData_.tides[i];
      char buf[48];
      bool isHigh = (t.type == "H");
      float ht = useMetric_ ? (float)(t.heightFt * 0.3048) : (float)t.heightFt;
      std::snprintf(buf, sizeof(buf), "%s  %s  %.2f%s", t.time.c_str(),
                    isHigh ? "HI" : "LO", ht, useMetric_ ? "m" : "ft");
      SDL_Color col = isHigh ? themes.accent : themes.textDim;
      cat->drawText(renderer, buf, x_ + pad, curY, col, FontStyle::Micro);
      curY += rowFontSize_ + 2;
    }
    curY += 2;
  }

  // Buoy section
  if (currentData_.buoyValid) {
    cat->drawText(renderer, "BUOY", x_ + pad, curY, themes.accent,
                  FontStyle::TinyBold);
    curY += subFontSize_ + 2;

    const auto &b = currentData_.buoy;
    char buf[64];

    if (b.waveHeightM >= 0) {
      float wh =
          useMetric_ ? (float)b.waveHeightM : (float)(b.waveHeightM * 3.281);
      std::snprintf(buf, sizeof(buf), "Waves: %.1f%s @ %.0fs", wh,
                    useMetric_ ? "m" : "ft",
                    b.wavePeriodS > 0 ? b.wavePeriodS : 0.0);
      cat->drawText(renderer, buf, x_ + pad, curY, themes.text, FontStyle::Micro);
      curY += rowFontSize_ + 2;
    }

    if (b.waterTempC > -900) {
      float wt =
          useMetric_ ? (float)b.waterTempC : (float)(b.waterTempC * 1.8 + 32);
      std::snprintf(buf, sizeof(buf), "Water: %.1f%s", wt,
                    useMetric_ ? "C" : "F");
      cat->drawText(renderer, buf, x_ + pad, curY, themes.info,
                    FontStyle::Micro);
      curY += rowFontSize_ + 2;
    }

    if (b.windSpeedMps >= 0) {
      float ws = useMetric_ ? (float)b.windSpeedMps
                            : (float)(b.windSpeedMps * 1.944); // kts
      std::snprintf(buf, sizeof(buf), "Wind: %.1f%s @ %d deg", ws,
                    useMetric_ ? "m/s" : "kt", b.windDirDeg);
      cat->drawText(renderer, buf, x_ + pad, curY, themes.textDim,
                    FontStyle::Micro);
    }
  }
}


void MarinePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::MicroBold);
  rowFontSize_ = cat->ptSize(FontStyle::Micro);
  subFontSize_ = cat->ptSize(FontStyle::TinyBold);
  menuFontSize_ = cat->ptSize(FontStyle::SmallBold);
}


bool MarinePanel::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (menuVisible_) {
    if (stationInput_.onMouseDown(mx, my, clicks, fontMgr_, stationRect_.x,
                                  stationRect_.y, stationRect_.w,
                                  stationRect_.h, FontStyle::SmallRegular, 6)) {
      activeField_ = 0;
      stationInput_.setActive(true);
      buoyInput_.setActive(false);
      return true;
    }
    if (buoyInput_.onMouseDown(mx, my, clicks, fontMgr_, buoyRect_.x,
                               buoyRect_.y, buoyRect_.w, buoyRect_.h,
                               FontStyle::SmallRegular, 6)) {
      activeField_ = 1;
      stationInput_.setActive(false);
      buoyInput_.setActive(true);
      return true;
    }

    return true; // Click eaten by modal
  }
  return false;
}

void MarinePanel::onMouseMove(int mx, int my) {
  if (menuVisible_) {
    if (activeField_ == 0) {
      stationInput_.onMouseMove(mx, fontMgr_, stationRect_.x, stationRect_.w,
                                FontStyle::SmallRegular, 6);
    } else {
      buoyInput_.onMouseMove(mx, fontMgr_, buoyRect_.x, buoyRect_.w,
                             FontStyle::SmallRegular, 6);
    }
  }
}

bool MarinePanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (menuVisible_) {
    stationInput_.onMouseUp();
    buoyInput_.onMouseUp();

    if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y &&
        my < okRect_.y + okRect_.h) {
      saveSettings();
      menuVisible_ = false;
      return true;
    }
    if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
        my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
      menuVisible_ = false;
      return true;
    }
    if (mx >= lookupRect_.x && mx < lookupRect_.x + lookupRect_.w &&
        my >= lookupRect_.y && my < lookupRect_.y + lookupRect_.h) {
      if (provider_) {
        auto &cfg = ConfigManager::instance().getConfig();
        provider_->lookupClosestStations(cfg.lat, cfg.lon);
        isSearching_ = true;
      }
      return true;
    }
    return true;
  }


  // Open menu on click
  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    // Top 10% -> widget selection (let PaneContainer handle it)
    if (my < y_ + height_ / 10)
      return false;

    auto &cfg = ConfigManager::instance().getConfig();
    stationInput_.setValue(cfg.marineStation);
    buoyInput_.setValue(cfg.marineBuoy);
    activeField_ = 0;
    stationInput_.setActive(true);
    buoyInput_.setActive(false);
    menuVisible_ = true;
    recalcMenuLayout();
    return true;
  }

  return false;
}

bool MarinePanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (menuVisible_) {
    if (key == SDLK_TAB) {
      activeField_ = (activeField_ + 1) % 2;
      stationInput_.setActive(activeField_ == 0);
      buoyInput_.setActive(activeField_ == 1);
      return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      saveSettings();
      menuVisible_ = false;
      return true;
    }
    if (key == SDLK_ESCAPE) {
      menuVisible_ = false;
      return true;
    }

    TextInput *ti = (activeField_ == 0) ? &stationInput_ : &buoyInput_;
    return ti->onKeyDown(key, mod);
  }
  return false;
}

bool MarinePanel::onTextInput(const char *text) {
  if (menuVisible_) {
    TextInput *ti = (activeField_ == 0) ? &stationInput_ : &buoyInput_;
    return ti->onTextInput(text);
  }
  return false;
}

void MarinePanel::renderModal(SDL_Renderer *renderer) {
  if (!menuVisible_)
    return;

  ThemeColors themes = getThemeColors(theme_);
  renderMenu(renderer, themes);
}

void MarinePanel::recalcMenuLayout() {
  int menuW = 200;
  int menuH = 170;
  menuRect_ = {x_ + (width_ - menuW) / 2, y_ + (height_ - menuH) / 2, menuW,
               menuH};
  
  // Ensure it doesn't go off top of screen if widget is near top
  if (menuRect_.y < 0) menuRect_.y = 2;


  int pad = 10;
  int curY = menuRect_.y + pad + 25;
  int fieldH = 22;
  int fieldW = menuW - 2 * pad;

  stationRect_ = {menuRect_.x + pad, curY, fieldW, fieldH};
  curY += fieldH + 20;
  buoyRect_ = {menuRect_.x + pad, curY, fieldW, fieldH};
  curY += fieldH + 12;



  int btnW = (menuW - 3 * pad) / 2;
  int btnH = 24;

  lookupRect_ = {menuRect_.x + pad, curY, fieldW, btnH};

  okRect_ = {menuRect_.x + pad, menuRect_.y + menuH - btnH - pad, btnW, btnH};
  cancelRect_ = {menuRect_.x + 2 * pad + btnW, menuRect_.y + menuH - btnH - pad,
                 btnW, btnH};
}


void MarinePanel::renderMenu(SDL_Renderer *renderer,
                             const ThemeColors &themes) {
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                         themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &menuRect_);

  cat->drawText(renderer, "Marine Setup", menuRect_.x + 10, menuRect_.y + 5,
                themes.accent, FontStyle::MicroBold);

  // Labels
  cat->drawText(renderer, "NOAA Station ID:", stationRect_.x,
                stationRect_.y - 12, themes.textDim, FontStyle::Micro);
  cat->drawText(renderer, "NDBC Buoy ID:", buoyRect_.x, buoyRect_.y - 12,
                themes.textDim, FontStyle::Micro);

  // Fields
  stationInput_.render(renderer, fontMgr_, stationRect_.x, stationRect_.y,
                       stationRect_.w, stationRect_.h, FontStyle::SmallRegular,
                       6, activeField_ == 0, true, themes.accent, themes.border,
                       themes.success, themes.text, themes.textDim,
                       "e.g. 8722670", &themes.rowStripe1);

  buoyInput_.render(renderer, fontMgr_, buoyRect_.x, buoyRect_.y, buoyRect_.w,
                    buoyRect_.h, FontStyle::SmallRegular, 6, activeField_ == 1,
                    true, themes.accent, themes.border, themes.success,
                    themes.text, themes.textDim, "e.g. 41114",
                    &themes.rowStripe1);

  // Buttons
  auto drawBtn = [&](const SDL_Rect &r, const char *label, SDL_Color bgCol,
                     bool hover) {
    if (hover) {
      SDL_SetRenderDrawColor(renderer, bgCol.r, bgCol.g, bgCol.b, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                             themes.rowStripe1.b, 255);
    }
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2,
                  hover ? themes.bg : themes.text, FontStyle::Micro, true,
                  false, true);
  };

  int mx, my;
  SDL_GetMouseState(&mx, &my);
  bool okHover = mx >= okRect_.x && mx < okRect_.x + okRect_.w &&
                 my >= okRect_.y && my < okRect_.y + okRect_.h;
  bool cancelHover = mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
                     my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h;
  bool lookupHover = mx >= lookupRect_.x && mx < lookupRect_.x + lookupRect_.w &&
                     my >= lookupRect_.y && my < lookupRect_.y + lookupRect_.h;

  drawBtn(okRect_, "Save", themes.success, okHover);
  drawBtn(cancelRect_, "Cancel", themes.accent, cancelHover);

  if (isSearching_) {
    drawBtn(lookupRect_, "Searching...", themes.info, false);
  } else {
    drawBtn(lookupRect_, "Find Closest", themes.accent, lookupHover);
  }
}


void MarinePanel::saveSettings() {
  auto &cfg = ConfigManager::instance().getConfig();
  cfg.marineStation = stationInput_.getValue();
  cfg.marineBuoy = buoyInput_.getValue();
  ConfigManager::instance().save(cfg);
  
  if (provider_) {
    provider_->fetch(cfg.marineStation, cfg.marineBuoy, true);
  }
  
  // No direct way to trigger fetch in provider from here without a pointer or observer.
  // The next update() loop or rotation will naturally pick up the new ID in main.cpp if we were to change how fetch is called,
  // but main.cpp usually handles the periodic fetch.
}

REGISTER_WIDGET("marine", "Marine", false, false, {
  return std::make_unique<MarinePanel>(0, 0, 0, 0, deps.fontMgr, deps.marineStore, deps.marineProvider);
})

