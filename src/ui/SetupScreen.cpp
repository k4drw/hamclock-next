#include "SetupScreen.h"
#include "../core/Astronomy.h"
#include "../core/ContestModeManager.h"
#include "../core/DisplayPower.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

SetupScreen::SetupScreen(int x, int y, int w, int h, FontManager &fontMgr,
                         BrightnessManager &brightnessMgr,
                         std::shared_ptr<DisplayPower> displayPower)
    : Widget(x, y, w, h), fontMgr_(fontMgr), brightnessMgr_(brightnessMgr),
      displayPower_(std::move(displayPower)) {
  LOG_D("SetupScreen", "Constructor: x={}, y={}, w={}, h={}", x, y, w, h);
  themeCustomizer_ = std::make_unique<HamClock::ThemeCustomizer>(
      x, y, w, h, fontMgr, theme_, colorOverrides_);
  recalcLayout();
}

void SetupScreen::recalcLayout() {
  // Logic moved to render() using FontStyle for better resolution independence
}

void SetupScreen::autoPopulateLatLon() {
  std::string g = gridInput_.getValue();
  for (size_t i = 0; i < g.size(); ++i) {
    if (i < 2) {
      if (g[i] >= 'a' && g[i] <= 'z')
        g[i] -= 32;
    } else if (i >= 4) {
      if (g[i] >= 'A' && g[i] <= 'Z')
        g[i] += 32;
    }
  }
  if (g != gridInput_.getValue())
    gridInput_.setValue(g);

  if (g.size() >= 4) {
    gridValid_ = Astronomy::gridToLatLon(g, gridLat_, gridLon_);
  } else {
    gridValid_ = false;
  }

  if (gridValid_ && !latLonManual_) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", gridLat_);
    latInput_.setValue(buf);
    std::snprintf(buf, sizeof(buf), "%.4f", gridLon_);
    lonInput_.setValue(buf);
  }
}

TextInput *SetupScreen::getActiveInput() {
  if (activeTab_ == Tab::Identity) {
    switch (activeField_) {
    case 0:
      return &callsignInput_;
    case 1:
      return &gridInput_;
    case 2:
      return &latInput_;
    case 3:
      return &lonInput_;
    }
  } else if (activeTab_ == Tab::Spotting) {
    switch (activeField_) {
    case 0:
      return &clusterHostInput_;
    case 1:
      return &clusterPortInput_;
    case 2:
      return &clusterLoginInput_;
    case 3:
      return &wsjtxPortInput_;
    }
  } else if (activeTab_ == Tab::Appearance) {
    switch (activeField_) {
    case 0:
      return &dimTimeInput_;
    case 1:
      return &brightTimeInput_;
    }
  } else if (activeTab_ == Tab::Services) {
    switch (activeField_) {
    case 0:
      return &qrzUsernameInput_;
    case 1:
      return &qrzPasswordInput_;
    case 2:
      return &repeaterBookInput_;
    case 3:
      return &winlinkInput_;
    }
  } else if (activeTab_ == Tab::Rig) {
    switch (activeField_) {
    case 0:
      return &rigHostInput_;
    case 1:
      return &rigPortInput_;
    case 2:
      return &rotatorHostInput_;
    case 3:
      return &rotatorPortInput_;
    }
  } else if (activeTab_ == Tab::Network) {
    switch (activeField_) {
    case 0:
      return &hubIpInput_;
    case 1:
      return &hubPortInput_;
    }
  } else if (activeTab_ == Tab::Widgets) {
    // Rotation interval field is actually numeric-only handled in onTextInput
    // but we can return nullptr as it's not a TextInput widget.
    return nullptr;
  } else if (activeTab_ == Tab::Watchlist) {
    return &watchlistInputField_;
  }
  return nullptr;
}

void SetupScreen::update() {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    themeCustomizer_->update();
    return;
  }
  autoPopulateLatLon();

  mismatchWarning_ = false;
  if (latLonManual_ && gridValid_ && !latInput_.getValue().empty() &&
      !lonInput_.getValue().empty()) {
    double manLat = StringUtils::safe_stod(latInput_.getValue());
    double manLon = StringUtils::safe_stod(lonInput_.getValue());
    double tolLat = (gridInput_.getValue().size() >= 6) ? 0.5 : 1.0;
    double tolLon = (gridInput_.getValue().size() >= 6) ? 1.0 : 2.0;
    if (std::fabs(manLat - gridLat_) > tolLat ||
        std::fabs(manLon - gridLon_) > tolLon) {
      mismatchWarning_ = true;
    }
  }
}

bool SetupScreen::isModalActive() const {
  return themeCustomizer_ && themeCustomizer_->isActive();
}

void SetupScreen::render(SDL_Renderer *renderer) {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    themeCustomizer_->render(renderer);
    return;
  }
  if (!fontMgr_.ready())
    return;

  auto *cat = fontMgr_.catalog();

  // Ensure layout is up-to-date if dimensions changed
  // Fixes case where setup is launched after window resize
  if (width_ != lastRenderWidth_ || height_ != lastRenderHeight_) {
    LOG_D("SetupScreen", "Dimensions changed, recalculating layout");
    recalcLayout();
    lastRenderWidth_ = width_;
    lastRenderHeight_ = height_;
  }

  // Main Modal Box — fill the entire app canvas
  int modalW = width_;
  int modalH = height_;
  int modalX = x_;
  int modalY = y_;
  modalRect_ = {modalX, modalY, modalW, modalH};

  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  // Background Fill (No extra dimming, use theme background)
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &modalRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &modalRect_);

  int cx = modalX + modalW / 2;
  int pad = 20;
  int fieldW = modalW - 2 * pad;
  int fieldX = modalX + pad;
  int fieldH = cat->ptSize(FontStyle::SmallRegular) + 14;
  int textPad = 7;

  SDL_Color titleCol = themes.text; // Use standard text color for title (was themes.accent/cyan)

  int y = modalRect_.y + pad;

  cat->drawText(renderer, "HamClock-Next Setup", cx,
                y + cat->ptSize(FontStyle::MediumBold) / 2, titleCol,
                FontStyle::MediumBold, true, false, true);
  y += cat->ptSize(FontStyle::MediumBold) +
       pad / 2; // tightened: was +pad, halved gap to tab bar

  // Appearance tab absorbs brightness/display settings — 9 tabs total
  const char *tabs[] = {"Identity", "Spotting",  "Appearance",
                        "Rig/Rot",  "Services",  "Network",
                        "Widgets",  "Watchlist", "Update"};
  int numTabs = 9;
  int tabW = fieldW / numTabs;

  // Calculate safe font size for tabs to prevent overflow
  // Longest label is "Appearance" (10 chars)
  int tabTextPad = 4; // Padding on each side
  int maxTabTextWidth = tabW - (tabTextPad * 2);
  FontStyle tabStyle = FontStyle::SmallRegular;

  // Reduce font size if labels won't fit
  int longestWidth = 0;
  for (int i = 0; i < numTabs; ++i) {
    int w = fontMgr_.getLogicalWidth(tabs[i], cat->ptSize(tabStyle));
    if (w > longestWidth)
      longestWidth = w;
  }
  if (longestWidth > maxTabTextWidth) {
    tabStyle = FontStyle::Fast;
  }

  for (int i = 0; i < numTabs; ++i) {
    SDL_Rect tr = {fieldX + i * tabW, y, tabW, fieldH};
    bool active = (int)activeTab_ == i;
    if (active) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 60);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                             themes.rowStripe1.b, 255);
    }
    SDL_RenderFillRect(renderer, &tr);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, active ? 255 : 80);
    SDL_RenderDrawRect(renderer, &tr);
    cat->drawText(renderer, tabs[i], tr.x + tabW / 2, tr.y + fieldH / 2,
                  active ? themes.text : themes.text, tabStyle, true, false,
                  true);
  }
  y += fieldH + pad / 2;

  switch (activeTab_) {
  case Tab::Identity:
    renderTabIdentity(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Spotting:
    renderTabDXCluster(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Appearance:
    renderTabAppearance(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Rig:
    renderTabRig(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Services:
    renderTabServices(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Network:
    renderTabNetwork(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Widgets:
    renderTabWidgets(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Watchlist:
    renderTabWatchlist(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  case Tab::Update:
    renderTabUpdate(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  }

  // Footer Buttons (Sticky at bottom)
  y = modalY + modalH - 12 - 40;

  // Draw footer background
  SDL_Rect footerBg = {modalX + 2, y - 10, modalW - 4,
                       modalH - (y - modalY - 10)};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                         themes.rowStripe2.b, 255);
  SDL_RenderFillRect(renderer, &footerBg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, modalX, y - 10, modalX + modalW, y - 10);

  int btnW = 100;
  int btnH = 34;

  // Cancel Button
  SDL_Rect cancelBtn = {cx - btnW - 20, y, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelBtn);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &cancelBtn);
  cat->drawText(renderer, "Cancel", cancelBtn.x + btnW / 2,
                cancelBtn.y + btnH / 2, themes.bg, FontStyle::SmallRegular,
                true, false, true);
  cancelBtnRect_ = cancelBtn;

  // Done Button
  SDL_Rect okBtn = {cx + 20, y, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &okBtn);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &okBtn);
  cat->drawText(renderer, "Done", okBtn.x + btnW / 2, okBtn.y + btnH / 2,
                themes.bg, FontStyle::SmallRegular, true, false, true);
  okBtnRect_ = okBtn;

#ifndef __EMSCRIPTEN__
  renderFontModal(renderer);
#endif
}

// renderTabIdentity — defined in SetupScreen_Identity.cpp

// renderTabDXCluster — defined in SetupScreen_Spotting.cpp

// renderTabAppearance — defined in SetupScreen_Appearance.cpp

// renderTabServices — defined in SetupScreen_Services.cpp

// renderTabNetwork — defined in SetupScreen_Network.cpp

// renderTabRig — defined in SetupScreen_Rig.cpp

// renderTabWidgets — defined in SetupScreen_Widgets.cpp

// renderTabUpdate — defined in SetupScreen_Update.cpp

void SetupScreen::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  if (themeCustomizer_) {
    themeCustomizer_->onResize(x, y, w, h);
  }
  recalcLayout();
}

// onMouseDown, onMouseUp, onMouseWheel, onMouseMove, onKeyDown, onTextInput — defined in SetupScreen_Events.cpp

// setConfig, getConfig, getActions, getActionRect — defined in SetupScreen_Config.cpp

void SetupScreen::addWatchlistEntriesFromInput() {
  std::string input = watchlistInputField_.getValue();
  if (input.empty())
    return;

  // Split by comma and/or space
  size_t start = 0;
  size_t end = input.find_first_of(", ");
  while (start != std::string::npos) {
    std::string token = input.substr(
        start, (end == std::string::npos) ? std::string::npos : end - start);

    // Trim and uppercase
    std::string call = StringUtils::trim(token);
    std::transform(call.begin(), call.end(), call.begin(), ::toupper);

    // Validate: alphanumeric or slash only
    bool valid = !call.empty();
    for (char c : call) {
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/')) {
        valid = false;
        break;
      }
    }

    if (valid) {
      if (std::find(watchlistEntries_.begin(), watchlistEntries_.end(), call) ==
          watchlistEntries_.end()) {
        watchlistEntries_.push_back(call);
      }
    }

    if (end == std::string::npos)
      break;
    start = input.find_first_not_of(", ", end);
    end = input.find_first_of(", ", start);
  }

  watchlistInputField_.clear();
}
