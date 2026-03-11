#include "SetupScreen.h"
#include "../core/Astronomy.h"
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
    double manLat = std::atof(latInput_.getValue().c_str());
    double manLon = std::atof(lonInput_.getValue().c_str());
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

  // Dim background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
  SDL_Rect full = {0, 0, width_, height_};
  SDL_RenderFillRect(renderer, &full);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Main Modal Box — fill the entire app canvas
  int modalW = width_;
  int modalH = height_;
  int modalX = x_;
  int modalY = y_;
  modalRect_ = {modalX, modalY, modalW, modalH};

  ThemeColors themes = getThemeColors(theme_, &colorOverrides_);

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

  SDL_Color cyan = themes.accent;

  int y = modalRect_.y + pad;

  cat->drawText(renderer, "HamClock-Next Setup", cx,
                y + cat->ptSize(FontStyle::MediumBold) / 2, cyan,
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
                  active ? themes.text : themes.textDim, tabStyle, true, false,
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
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &cancelBtn);
  cat->drawText(renderer, "Cancel", cancelBtn.x + btnW / 2,
                cancelBtn.y + btnH / 2, themes.text, FontStyle::SmallRegular,
                true, false, true);
  cancelBtnRect_ = cancelBtn;

  // Done Button
  SDL_Rect okBtn = {cx + 20, y, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &okBtn);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &okBtn);
  cat->drawText(renderer, "Done", okBtn.x + btnW / 2, okBtn.y + btnH / 2,
                themes.text, FontStyle::SmallRegular, true, false, true);
  okBtnRect_ = okBtn;
}

void SetupScreen::renderTabIdentity(SDL_Renderer *renderer, int cx, int pad,
                                    int fieldW, int fieldH, int fieldX,
                                    int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color green = {0, 200, 0, 255};
  SDL_Color red = {255, 80, 80, 255};

  int labelH = cat->ptSize(FontStyle::SmallBold) + 4;

  cat->drawText(renderer, "Callsign:", fieldX, y, white, FontStyle::SmallBold);
  y += labelH;
  callsignRect_ = {fieldX, y, fieldW, fieldH};
  callsignInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                        FontStyle::SmallRegular, textPad, activeField_ == 0,
                        !callsignInput_.getValue().empty(), orange, gray, white,
                        white, gray, "e.g. K4DRW");
  y += fieldH + vSpace;

  cat->drawText(renderer, "Grid Square:", fieldX, y, white,
                FontStyle::SmallBold);
  y += labelH;
  gridRect_ = {fieldX, y, fieldW, fieldH};
  gridInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                    FontStyle::SmallRegular, textPad, activeField_ == 1,
                    gridValid_, orange, gray, green, white, gray,
                    "e.g. EL87qr");
  y += fieldH + vSpace;

  int halfFieldW = (fieldW - pad) / 2;
  int labelY = y;
  cat->drawText(renderer, "Latitude:", fieldX, labelY, white,
                FontStyle::SmallBold);
  cat->drawText(renderer, "Longitude:", fieldX + halfFieldW + pad, labelY,
                white, FontStyle::SmallBold);

  // Mismatch or Auto-calc status: right-aligned to the Latitude input's right
  // edge
  if (mismatchWarning_) {
    cat->drawText(renderer, "Warning: Lat/Lon mismatch!",
                  fieldX + halfFieldW / 2, labelY + 2, red, FontStyle::Fast,
                  true, false, true);
  } else if (gridValid_ && !latLonManual_) {
    cat->drawText(renderer, "Auto-calculated", fieldX + halfFieldW / 2,
                  labelY + 2, gray, FontStyle::Fast, true, false, true);
  }

  latRect_ = {fieldX, y, halfFieldW, labelH + fieldH};
  lonRect_ = {fieldX + halfFieldW + pad, y, halfFieldW, labelH + fieldH};
  y += labelH;

  int latY = y;
  latInput_.render(renderer, fontMgr_, fieldX, latY, halfFieldW, fieldH,
                   FontStyle::SmallRegular, textPad, activeField_ == 2,
                   !latInput_.getValue().empty(), orange, gray, white, white,
                   gray, "e.g. 27.76");

  int lonY = y;
  lonInput_.render(renderer, fontMgr_, fieldX + halfFieldW + pad, lonY,
                   halfFieldW, fieldH, FontStyle::SmallRegular, textPad,
                   activeField_ == 3, !lonInput_.getValue().empty(), orange,
                   gray, white, white, gray, "e.g. -82.64");
  y = std::max(latY, lonY) + pad / 2 + pad;

  ThemeColors themes = getThemeColors(theme_, &colorOverrides_);
  gpsToggleRect_ = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &gpsToggleRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &gpsToggleRect_);
  if (gpsEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Synchronize with GPS (gpsd)", fieldX + 30, y + 10,
                white, FontStyle::SmallRegular, false, false, true);
}

void SetupScreen::renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad,
                                     int fieldW, int fieldH, int fieldX,
                                     int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = 5;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  // --- DX CLUSTER SECTION ---
  cat->drawText(renderer, "--- DX Cluster ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  cat->drawText(renderer, "Host:", fieldX, y, white, FontStyle::SmallBold);
  cat->drawText(renderer, "Port:", fieldX + fieldW / 2 + pad, y, white,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  int halfW = (fieldW - pad) / 2;
  int hostY = y;
  clusterHostInput_.render(renderer, fontMgr_, fieldX, hostY, halfW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 0,
                           !clusterHostInput_.getValue().empty(), orange, gray,
                           white, white, gray, "dxusa.net");
  clusterHostRect_ = {fieldX, hostY, halfW, fieldH};
  int portY = y;
  clusterPortInput_.render(renderer, fontMgr_, fieldX + halfW + pad, portY,
                           halfW, fieldH, FontStyle::SmallRegular, textPad,
                           activeField_ == 1,
                           !clusterPortInput_.getValue().empty(), orange, gray,
                           white, white, gray, "7300");
  clusterPortRect_ = {fieldX + halfW + pad, portY, halfW, fieldH};
  y += fieldH + vSpace;

  cat->drawText(renderer, "Login:", fieldX, y, white, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;
  clusterLoginInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            !clusterLoginInput_.getValue().empty(), orange,
                            gray, white, white, gray, "NOCALL");
  clusterLoginRect_ = {fieldX, y, fieldW, fieldH};
  y += fieldH + vSpace;

  ThemeColors themes = getThemeColors(theme_, &colorOverrides_);
  // Toggles row 1
  SDL_Rect enableToggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &enableToggle);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &enableToggle);
  if (clusterEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Enable DX Cluster", fieldX + 30, y + 10, white,
                FontStyle::SmallRegular, false, false, true);
  clusterToggleRect_ = enableToggle;

  y += 24;

  SDL_Rect toggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &toggle);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &toggle);
  if (clusterWSJTX_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Use WSJT-X (UDP)", fieldX + 30, y + 10, white,
                FontStyle::SmallRegular, false, false, true);
  toggleRect_ = toggle;
  if (clusterWSJTX_) {
    int wPortW = std::min(120, halfW);
    int portX = fieldX + halfW + pad;
    wsjtxPortRect_ = {portX, y, wPortW, fieldH};
    wsjtxPortInput_.render(renderer, fontMgr_, portX, y, wPortW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 3,
                           !wsjtxPortInput_.getValue().empty(), orange, gray,
                           white, white, gray, "2237");
  } else {
    wsjtxPortRect_ = {0, 0, 0, 0};
  }
  y += 30;

  // --- RBN SECTION ---
  cat->drawText(renderer, "--- Reverse Beacon Network ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  SDL_Rect rbnToggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rbnToggle);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rbnToggle);
  if (rbnEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Enable RBN (feeds DX Cluster panel)", fieldX + 30,
                y + 10, white, FontStyle::SmallRegular, false, false, true);
  rbnToggleRect_ = rbnToggle;
  y += 24;
}

void SetupScreen::renderTabAppearance(SDL_Renderer *renderer, int cx, int pad,
                                      int fieldW, int fieldH, int fieldX,
                                      int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  // Cap vSpace so Appearance tab fits above the button bar on small screens
  // (e.g. 1024x600 7" display). Available height / ~26 keeps all rows visible.
  int availH = (y_ + height_ - 52) - y;
  int vSpace = std::min(pad / 2, std::max(2, availH / 26));
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color cyan = {0, 200, 255, 255};
  int halfW = fieldW / 2;
  ThemeColors themes = getThemeColors(theme_, &colorOverrides_);

  // --- Appearance section ---
  cat->drawText(renderer, "--- Appearance ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  auto drawBtn = [&](const SDL_Rect &r, const std::string &txt) {
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, txt, r.x + r.w / 2, r.y + r.h / 2, themes.text,
                  FontStyle::Fast, true, false, true);
  };

  // Row 1: Theme and Customize (Centered)
  int wThemeLabel =
      fontMgr_.getLogicalWidth("Theme:", cat->ptSize(FontStyle::SmallRegular));
  int wRow1 = wThemeLabel + 10 + 80 + 15 + 85;
  int xRow1 = cx - wRow1 / 2;
  cat->drawText(renderer, "Theme:", xRow1, y + 12, white,
                FontStyle::SmallRegular, false, false, true);
  themeRect_ = {xRow1 + wThemeLabel + 10, y, 80, 24};
  customizeBtnRect_ = {themeRect_.x + 95, y, 85, 24};
  drawBtn(themeRect_, theme_);
  drawBtn(customizeBtnRect_, "Customize");
  y += 24 + vSpace;

  // Row 2: Toggles (Night Lights, Metric) - Centered
  auto drawToggle = [&](SDL_Rect &r, bool val, const char *lbl) {
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r);
    if (val) {
      SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
      SDL_Rect check = {r.x + 4, r.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, lbl, r.x + 25, r.y + r.h / 2, themes.text,
                  FontStyle::SmallRegular, false, false, true);
  };

  int wNL = 25 + fontMgr_.getLogicalWidth("Night Lights",
                                          cat->ptSize(FontStyle::SmallRegular));
  int wMU = 25 + fontMgr_.getLogicalWidth("Metric Units",
                                          cat->ptSize(FontStyle::SmallRegular));
  int toggleGap = 30;
  int wRow2 = wNL + toggleGap + wMU;
  int xRow2 = cx - wRow2 / 2;

  nightLightsRect_ = {xRow2, y, 20, 20};
  drawToggle(nightLightsRect_, mapNightLights_, "Night Lights");
  metricToggleRect_ = {xRow2 + wNL + toggleGap, y, 20, 20};
  drawToggle(metricToggleRect_, useMetric_, "Metric Units");
  y += 20 + vSpace;

  // --- Brightness section ---
  cat->drawText(renderer, "--- Brightness ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  brightnessSliderRect_ = {fieldX, y, fieldW, fieldH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &brightnessSliderRect_);
  int brightness = brightnessMgr_.getBrightness();
  int brightW = (fieldW * brightness) / 100;
  SDL_Rect brightRect = {fieldX, y, brightW, fieldH};
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
  SDL_RenderFillRect(renderer, &brightRect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &brightnessSliderRect_);
  std::string brightText = std::to_string(brightness) + "%";
  cat->drawText(renderer, brightText, fieldX + fieldW / 2, y + fieldH / 2,
                white, FontStyle::SmallRegular, true, false, true);
  y += fieldH + vSpace;

  scheduleToggleRect_ = {fieldX, y, 20, 20};
  drawToggle(scheduleToggleRect_, brightnessMgr_.isScheduleEnabled(),
             "Enable Dim/Bright Schedule");
  y += 20 + vSpace;

  if (brightnessMgr_.isScheduleEnabled()) {
    cat->drawText(renderer, "Dim:", fieldX, y + 12, white,
                  FontStyle::SmallRegular, false, false, true);
    int halfFieldW = (fieldW - pad - 60) / 2;
    int dimX = fieldX + 40;
    dimTimeRect_ = {dimX, y, halfFieldW, 24};
    dimTimeInput_.render(renderer, fontMgr_, dimX, y, halfFieldW, 24,
                         FontStyle::SmallRegular, textPad, activeField_ == 1,
                         true, orange, gray, white, white, gray, "HH:MM");

    cat->drawText(renderer, "Bright:", fieldX + halfW, y + 12, white,
                  FontStyle::SmallRegular, false, false, true);
    int brightX = fieldX + halfW + 55;
    brightTimeRect_ = {brightX, y, halfFieldW, 24};
    brightTimeInput_.render(renderer, fontMgr_, brightX, y, halfFieldW, 24,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            true, orange, gray, white, white, gray, "HH:MM");
    y += 24 + vSpace;
  } else {
    dimTimeRect_ = {0, 0, 0, 0};
    brightTimeRect_ = {0, 0, 0, 0};
  }

  // Row 5: Display Power Method
  {
    int wLabel = fontMgr_.getLogicalWidth("Display Power:", cat->ptSize(FontStyle::SmallRegular));
    cat->drawText(renderer, "Display Power:", fieldX, y + 12, white,
                  FontStyle::SmallRegular, false, false, true);
    powerMethodRect_ = {fieldX + wLabel + 10, y, 160, 24};
    drawBtn(powerMethodRect_, displayPowerMethod_);
    y += 24 + vSpace;
  }
}

void SetupScreen::renderTabServices(SDL_Renderer *renderer, int cx, int pad,
                                    int fieldW, int fieldH, int fieldX,
                                    int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};

  cat->drawText(renderer, "QRZ Username:", fieldX, y, white,
                FontStyle::SmallBold);
  int labelH = cat->ptSize(FontStyle::SmallBold) + 4;
  qrzUsernameRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  qrzUsernameInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 0,
                           true, orange, gray, white, white, gray,
                           "e.g. K4DRW");
  y += fieldH + vSpace;

  cat->drawText(renderer, "QRZ Password:", fieldX, y, white,
                FontStyle::SmallBold);
  qrzPasswordRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  {
    std::string passMask(qrzPasswordInput_.getValue().length(), '*');
    TextInput tmpPwd;
    tmpPwd.setValue(passMask);
    if (activeField_ == 1) {
      tmpPwd.setCursorPos(qrzPasswordInput_.getCursorPos());
      tmpPwd.setSelectionAnchor(qrzPasswordInput_.getSelectionAnchor());
    }
    tmpPwd.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                  FontStyle::SmallRegular, textPad, activeField_ == 1, true,
                  orange, gray, white, white, gray, "********");
  }
  y += fieldH + vSpace;

  cat->drawText(renderer, "RepeaterBook Key:", fieldX, y, white,
                FontStyle::SmallBold);
  repeaterBookRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  repeaterBookInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            true, orange, gray, white, white, gray, "Key");
  y += fieldH + vSpace;

  cat->drawText(renderer, "Winlink Key:", fieldX, y, white,
                FontStyle::SmallBold);
  winlinkRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  winlinkInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 3,
                       true, orange, gray, white, white, gray, "Key");
  y += fieldH + vSpace;
}

void SetupScreen::renderTabNetwork(SDL_Renderer *renderer, int /*cx*/, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};

  cat->drawText(renderer, "--- Local Data Hub ---", fieldX, y, orange,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad;

  // Mode cycle button
  const char *modeLabel = (hubMode_ == HubMode::Master)   ? "Master"
                          : (hubMode_ == HubMode::Client) ? "Client"
                                                          : "Off";
  int btnW = 80;
  hubModeRect_ = {fieldX + fieldW - btnW, y, btnW, fieldH};
  ThemeColors themes = getThemeColors(theme_, &colorOverrides_);
  cat->drawText(renderer, "Mode:", fieldX,
                y + (fieldH - cat->ptSize(FontStyle::SmallRegular)) / 2, white,
                FontStyle::SmallRegular);
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &hubModeRect_);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &hubModeRect_);
  cat->drawText(renderer, modeLabel, hubModeRect_.x + hubModeRect_.w / 2,
                hubModeRect_.y + hubModeRect_.h / 2, orange,
                FontStyle::SmallRegular, true, false, true);
  y += fieldH + vSpace;

  if (hubMode_ == HubMode::Client) {
    int labelH = cat->ptSize(FontStyle::SmallRegular) + 4;
    cat->drawText(renderer, "Hub IP:", fieldX, y, white,
                  FontStyle::SmallRegular);
    hubIpRect_ = {fieldX, y, fieldW, labelH + fieldH};
    y += labelH;
    hubIpInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 0,
                       true, orange, gray, white, white, gray,
                       "e.g. 192.168.1.100");
    y += fieldH + vSpace;

    cat->drawText(renderer, "Hub Port:", fieldX, y, white,
                  FontStyle::SmallRegular);
    hubPortRect_ = {fieldX, y, fieldW, labelH + fieldH};
    y += labelH;
    hubPortInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                         FontStyle::SmallRegular, textPad, activeField_ == 1,
                         true, orange, gray, white, white, gray, "8080");
    y += fieldH + vSpace;
  } else {
    hubIpRect_ = {0, 0, 0, 0};
    hubPortRect_ = {0, 0, 0, 0};
  }

  if (hubMode_ == HubMode::Master) {
    cat->drawText(renderer, "This instance serves cached data to hub clients.",
                  fieldX, y + vSpace, gray, FontStyle::Fast);
  } else if (hubMode_ == HubMode::Client) {
    cat->drawText(renderer, "Fetches via hub; falls back to direct after 2s.",
                  fieldX, y + vSpace, gray, FontStyle::Fast);
  } else {
    cat->drawText(renderer, "Hub mode disabled.", fieldX, y + vSpace, gray,
                  FontStyle::Fast);
  }
}

void SetupScreen::renderTabRig(SDL_Renderer *renderer, int cx, int pad,
                               int fieldW, int fieldH, int fieldX,
                               int textPad) {
  auto *cat = fontMgr_.catalog();
  int yBase =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int y = yBase;
  int vSpace = pad / 2;
  int colW = fieldW / 2 - pad;
  int rightX = cx + pad / 2;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};

  // --- Rig Section (Left Column) ---
  cat->drawText(renderer, "Rig / CAT Control:", fieldX, y, white,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  int rigLabelH = cat->ptSize(FontStyle::SmallRegular) + 4;
  cat->drawText(renderer, "rigctld Host:", fieldX, y, white,
                FontStyle::SmallRegular);
  rigHostRect_ = {fieldX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rigHostInput_.render(renderer, fontMgr_, fieldX, y, colW, fieldH,
                       FontStyle::SmallRegular, textPad,
                       (activeTab_ == Tab::Rig && activeField_ == 0), true,
                       orange, gray, white, white, gray, "localhost");
  y += fieldH + vSpace;

  cat->drawText(renderer, "rigctld Port:", fieldX, y, white,
                FontStyle::SmallRegular);
  rigPortRect_ = {fieldX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rigPortInput_.render(renderer, fontMgr_, fieldX, y, colW, fieldH,
                       FontStyle::SmallRegular, textPad,
                       (activeTab_ == Tab::Rig && activeField_ == 1), true,
                       orange, gray, white, white, gray, "4532");
  y += fieldH + vSpace;

  // Auto-tune Toggle
  {
    SDL_Rect r = {fieldX, y, 20, 20};
    toggleRect_ = r;
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &r);
    if (rigAutoTune_) {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_Rect check = {r.x + 4, r.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Auto-Tune on Spot click", fieldX + 30, y + 10,
                  white, FontStyle::SmallRegular, false, false, true);
  }

  // --- Rotator Section (Right Column) ---
  y = yBase;
  cat->drawText(renderer, "Rotator Control:", rightX, y, white,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  cat->drawText(renderer, "rotctld Host:", rightX, y, white,
                FontStyle::SmallRegular);
  rotatorHostRect_ = {rightX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rotatorHostInput_.render(renderer, fontMgr_, rightX, y, colW, fieldH,
                           FontStyle::SmallRegular, textPad,
                           (activeTab_ == Tab::Rig && activeField_ == 2), true,
                           orange, gray, white, white, gray, "localhost");
  y += fieldH + vSpace;

  cat->drawText(renderer, "rotctld Port:", rightX, y, white,
                FontStyle::SmallRegular);
  rotatorPortRect_ = {rightX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rotatorPortInput_.render(renderer, fontMgr_, rightX, y, colW, fieldH,
                           FontStyle::SmallRegular, textPad,
                           (activeTab_ == Tab::Rig && activeField_ == 3), true,
                           orange, gray, white, white, gray, "4533");
  y += fieldH + vSpace;

  // Auto-track and Upover Toggles
  {
    // Auto-track
    SDL_Rect r1 = {rightX, y, 20, 20};
    rotatorAutoTrackRect_ = r1;
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &r1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &r1);
    if (rotatorAutoTrack_) {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_Rect check = {r1.x + 4, r1.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Auto-track Satellite", rightX + 30, y + 10, white,
                  FontStyle::SmallRegular, false, false, true);
    y += 24 + vSpace;

    // Upover
    SDL_Rect r2 = {rightX, y, 20, 20};
    rotatorUpoverRect_ = r2;
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &r2);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &r2);
    if (rotatorUpover_) {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_Rect check = {r2.x + 4, r2.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Upover Mode (Az flip)", rightX + 30, y + 10, white,
                  FontStyle::SmallRegular, false, false, true);
  }

  y = yBase + 210; // Bottom of tab area, adjusted for clarity
  cat->drawText(renderer, "Requires 'rigctld'/'rotctld' (Hamlib) daemon.", cx,
                y, gray, FontStyle::Fast, true);
}

void SetupScreen::renderTabWidgets(SDL_Renderer *renderer, int cx, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int /*textPad*/) {
  auto *cat = fontMgr_.catalog();
  // Clamp activePane_ to top-bar panes only (0-3)
  if (activePane_ > 3)
    activePane_ = 0;

  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  cat->drawText(renderer, "Select Active Widgets for Each Pane:", fieldX, y,
                white, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad / 2;

  // 4 top-bar pane buttons in one compact row
  const int btnH = 22;
  const int btnGap = 4;
  int paneW = fieldW / 4;
  static const char *kTopLabels[] = {"Top 1", "Top 2", "Top 3", "Top 4"};
  for (int i = 0; i < 4; ++i) {
    SDL_Rect pr = {fieldX + i * paneW, y, paneW - btnGap, btnH};
    bool active = activePane_ == i;
    SDL_SetRenderDrawColor(renderer, active ? 60 : 30, active ? 60 : 30,
                           active ? 80 : 40, 255);
    SDL_RenderFillRect(renderer, &pr);
    SDL_SetRenderDrawColor(renderer, active ? 0 : 200, active ? 200 : 80,
                           active ? 255 : 80, 255);
    SDL_RenderDrawRect(renderer, &pr);
    cat->drawText(renderer, kTopLabels[i], pr.x + pr.w / 2, pr.y + pr.h / 2,
                  active ? white : gray, FontStyle::Fast, true, false, true);
  }
  y += btnH + btnGap;

  // Sync rotation checkbox and Interval
  syncRotationRect_ = {fieldX, y, 16, 16};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &syncRotationRect_);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &syncRotationRect_);
  if (syncRotation_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect check = {syncRotationRect_.x + 3, syncRotationRect_.y + 3, 10, 10};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Sync pane rotation", fieldX + 22, y + 8, gray,
                FontStyle::Fast, false, false, true);

  cat->drawText(renderer, "Interval (s):", cx + 20, y + 8, gray,
                FontStyle::Fast, false, false, true);
  rotationToggleRect_ = {cx + 105, y, 40, 18};
  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_RenderFillRect(renderer, &rotationToggleRect_);
  SDL_SetRenderDrawColor(
      renderer, (activeTab_ == Tab::Widgets && activeField_ == 0) ? 255 : 80,
      (activeTab_ == Tab::Widgets && activeField_ == 0) ? 165 : 80,
      (activeTab_ == Tab::Widgets && activeField_ == 0) ? 0 : 80, 255);
  SDL_RenderDrawRect(renderer, &rotationToggleRect_);
  cat->drawText(renderer, std::to_string(rotationInterval_),
                rotationToggleRect_.x + rotationToggleRect_.w / 2,
                rotationToggleRect_.y + rotationToggleRect_.h / 2, white,
                FontStyle::Fast, true, false, true);
  y += 20;

  // --- Widget checklist (scrollable) ---
  widgetRects_.clear();
  int rowH = cat->ptSize(FontStyle::Fast) + 4;

  // Side panel section height: header + 4 radio options
  int sideSecH = cat->ptSize(FontStyle::SmallBold) + 4 + 4 * (rowH + 2) + 4;
  int footerY = modalRect_.y + modalRect_.h - 52;
  int sideSecY = footerY - sideSecH;
  int listEndY = sideSecY - pad / 2;
  int listAvailH = listEndY - y;

  static const WidgetType kAllTypes[] = {WidgetType::ADIF,
                                         WidgetType::ASTEROID,
                                         WidgetType::AURORA,
                                         WidgetType::AURORA_GRAPH,
                                         WidgetType::BAND_CONDITIONS,
                                         WidgetType::CALLBOOK,
                                         WidgetType::CLOCK_AUX,
                                         WidgetType::CONTESTS,
                                         WidgetType::COUNTDOWN,
                                         WidgetType::DE_WEATHER,
                                         WidgetType::DRAP,
                                         WidgetType::DST_INDEX,
                                         WidgetType::DX_CLUSTER,
                                         WidgetType::DX_PEDITIONS,
                                         WidgetType::DX_WEATHER,
                                         WidgetType::EME_TOOL,
                                         WidgetType::FORECAST,
                                         WidgetType::GIMBAL,
                                         WidgetType::HISTORY_KP,
                                         WidgetType::LIVE_SPOTS,
                                         WidgetType::MARINE,
                                         WidgetType::MOON,
                                         WidgetType::NCDXF,
                                         WidgetType::ON_THE_AIR,
                                         WidgetType::REMINDER,
                                         WidgetType::SANTA_TRACKER,
                                         WidgetType::SDO,
                                         WidgetType::SOLAR,
                                         WidgetType::HISTORY_FLUX,
                                         WidgetType::STOPWATCH,
                                         WidgetType::HISTORY_SSN,
                                         WidgetType::SYS_INFO,
                                         WidgetType::HURRICANE,
                                         WidgetType::WATCHLIST,
                                         WidgetType::ALERTS};
  constexpr int kNWidgets =
      static_cast<int>(sizeof(kAllTypes) / sizeof(kAllTypes[0]));
  constexpr int kColItems = (kNWidgets + 2) / 3; // items per column = 12

  int visRows = std::max(1, listAvailH / rowH);
  widgetListMaxScroll_ = std::max(0, kColItems - visRows);
  widgetListScrollOffset_ =
      std::max(0, std::min(widgetListScrollOffset_, widgetListMaxScroll_));
  widgetListStartY_ = y;
  widgetListEndY_ = y + visRows * rowH;

  // Clip widget list to its allotted area
  SDL_Rect prevClip = {0, 0, 0, 0};
  SDL_RenderGetClipRect(renderer, &prevClip);
  SDL_Rect listClip = {modalRect_.x + 2, y, modalRect_.w - 4, visRows * rowH};
  SDL_RenderSetClipRect(renderer, &listClip);

  int colW = fieldW / 3;
  const auto &currentPane = paneRotations_[activePane_];

  for (int col = 0; col < 3; ++col) {
    for (int row = 0; row < visRows; ++row) {
      int idx = col * kColItems + widgetListScrollOffset_ + row;
      if (idx >= kNWidgets)
        break;
      WidgetType t = kAllTypes[idx];

      bool allowed = true;
      if (activePane_ == 3) {
        allowed = (t == WidgetType::NCDXF || t == WidgetType::SOLAR ||
                   t == WidgetType::DX_WEATHER || t == WidgetType::DE_WEATHER);
      }

      int drawX = fieldX + col * colW;
      int drawY = y + row * rowH;
      SDL_Rect r = {drawX, drawY, 16, 16};
      SDL_SetRenderDrawColor(renderer, 45, 45, 48, 255);
      SDL_RenderFillRect(renderer, &r);
      SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
      SDL_RenderDrawRect(renderer, &r);

      if (allowed) {
        bool selected = std::find(currentPane.begin(), currentPane.end(), t) !=
                        currentPane.end();
        if (selected) {
          SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
          SDL_Rect check = {r.x + 3, r.y + 3, 10, 10};
          SDL_RenderFillRect(renderer, &check);
        }
        cat->drawText(renderer, widgetTypeDisplayName(t), r.x + 22, r.y + 8,
                      white, FontStyle::Fast, false, false, true);
        widgetRects_.push_back({t, r});
      } else {
        cat->drawText(renderer, widgetTypeDisplayName(t), r.x + 22, r.y + 8,
                      {60, 60, 70, 255}, FontStyle::Fast, false, false, true);
      }
    }
  }

  // Restore previous clip
  if (prevClip.w > 0 && prevClip.h > 0)
    SDL_RenderSetClipRect(renderer, &prevClip);
  else
    SDL_RenderSetClipRect(renderer, nullptr);

  // Scroll arrows when list overflows
  if (widgetListMaxScroll_ > 0) {
    int arrowX = modalRect_.x + modalRect_.w - 18;
    if (widgetListScrollOffset_ > 0)
      cat->drawText(renderer, "^", arrowX, y + 2, gray, FontStyle::Fast, true);
    if (widgetListScrollOffset_ < widgetListMaxScroll_)
      cat->drawText(renderer, "v", arrowX, widgetListEndY_ - rowH, gray,
                    FontStyle::Fast, true);
  }

  // --- Side Panel section ---
  y = sideSecY;
  cat->drawText(renderer, "--- Side Panel ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  // Determine current side-panel mode from paneRotations_[4/5]
  int curMode = 0;
  if (!paneRotations_[4].empty()) {
    WidgetType t5 = paneRotations_[4][0];
    if (t5 == WidgetType::DX_CLUSTER)
      curMode = 1;
    else if (t5 == WidgetType::ON_THE_AIR)
      curMode = 2;
    else if (t5 == WidgetType::LIVE_SPOTS)
      curMode = 3;
    // else curMode = 0 (DE_INFO + DX_INFO or default)
  }

  static const char *kSideLabels[] = {
      "DE Info + DX/Sat (2 panes, original)",
      "DX Cluster (full height)",
      "On The Air (full height)",
      "Live Spots (full height)",
  };
  for (int i = 0; i < 4; ++i) {
    SDL_Rect rr = {fieldX, y, 14, 14};
    sidePanelModeRects_[i] = {fieldX, y, fieldW, rowH};
    SDL_SetRenderDrawColor(renderer, 45, 45, 48, 255);
    SDL_RenderFillRect(renderer, &rr);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &rr);
    if (curMode == i) {
      SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
      SDL_Rect inner = {rr.x + 3, rr.y + 3, 8, 8};
      SDL_RenderFillRect(renderer, &inner);
    }
    cat->drawText(renderer, kSideLabels[i], rr.x + 22, rr.y + 7,
                  curMode == i ? white : gray, FontStyle::Fast, false, false,
                  true);
    y += rowH + 2;
  }
}

void SetupScreen::renderTabUpdate(SDL_Renderer *renderer, int cx, int pad,
                                  int fieldW, int fieldH, int fieldX, int) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  cat->drawText(renderer, "--- Update Status ---", cx, y, cyan,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + pad;

  cat->drawText(renderer, "Current Version:", fieldX, y, white,
                FontStyle::SmallRegular);
  int lenV = fontMgr_.getLogicalWidth(HAMCLOCK_VERSION,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_VERSION, fieldX + fieldW - lenV, y, gray,
                FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Platform Architecture:", fieldX, y, white,
                FontStyle::SmallRegular);
  int lenA = fontMgr_.getLogicalWidth(HAMCLOCK_ARCH,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_ARCH, fieldX + fieldW - lenA, y, gray,
                FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Installation Type:", fieldX, y, white,
                FontStyle::SmallRegular);
  int lenI = fontMgr_.getLogicalWidth(HAMCLOCK_INSTALL_TYPE,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_INSTALL_TYPE, fieldX + fieldW - lenI, y,
                gray, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + pad * 2;

  // Implementation-specific instructions
  std::string instr;
  std::string cmd;
  std::string type = HAMCLOCK_INSTALL_TYPE;

  if (type == "RPM") {
    instr = "An update is available via DNF.";
    cmd = "sudo dnf update hamclock-next";
  } else if (type == "DEB") {
    instr = "Download the latest .deb and install it.";
    cmd = "sudo apt install ./hamclock-next.deb";
  } else if (type == "WASM") {
    instr = "A new version of HamClock-Next is available.";
    cmd = "Please reload the page to update.";
  } else {
    instr = "Binary update available.";
    cmd = "Download the latest release from GitHub.";
  }

  cat->drawText(renderer, instr, cx, y, white, FontStyle::SmallRegular, true);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;
  cat->drawText(renderer, cmd, cx, y, cyan, FontStyle::SmallBold, true);
}

void SetupScreen::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  if (themeCustomizer_) {
    themeCustomizer_->onResize(x, y, w, h);
  }
  recalcLayout();
}

bool SetupScreen::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    return themeCustomizer_->onMouseDown(mx, my, mod, clicks);
  }

  auto *cat = fontMgr_.catalog();
  int pad = 20;
  int fieldW = modalRect_.w - 2 * pad;
  int fieldX = modalRect_.x + pad;
  int fieldH = cat->ptSize(FontStyle::SmallRegular) + 14;
  int textPad = 7;

  // 1. Tab Switching (Highest Priority)
  int yTabs = modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 3 * pad / 2;
  if (my >= yTabs && my <= yTabs + fieldH) {
    const Tab tabValues[] = {Tab::Identity, Tab::Spotting,  Tab::Appearance,
                             Tab::Rig,      Tab::Services,  Tab::Network,
                             Tab::Widgets,  Tab::Watchlist, Tab::Update};
    int numTabs = 9;
    int tabW = fieldW / numTabs;
    for (int i = 0; i < numTabs; ++i) {
      if (mx >= fieldX + i * tabW && mx <= fieldX + (i + 1) * tabW) {
        activeTab_ = tabValues[i];
        activeField_ = 0;
        TextInput *ti = getActiveInput();
        if (ti) {
          ti->setActive(true);
          ti->setCursorToEnd();
        }
        return true;
      }
    }
  }

  // Helper for field hit-testing
  auto hitField = [&](SDL_Rect &r, int idx, TextInput *ti) -> bool {
    if (r.w > 0 && mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
      activeField_ = idx;
      if (ti) {
        ti->setActive(true);
        ti->onMouseDown(mx, my, clicks, fontMgr_, r.x, r.y, r.w, r.h,
                        FontStyle::SmallRegular, textPad);
      }
      return true;
    }
    return false;
  };

  // 2. Tab-Specific Field Interaction
  if (activeTab_ == Tab::Identity) {
    if (hitField(callsignRect_, 0, &callsignInput_))
      return true;
    if (hitField(gridRect_, 1, &gridInput_))
      return true;
    if (hitField(latRect_, 2, &latInput_))
      return true;
    if (hitField(lonRect_, 3, &lonInput_))
      return true;
  } else if (activeTab_ == Tab::Spotting) {
    if (hitField(clusterHostRect_, 0, &clusterHostInput_))
      return true;
    if (hitField(clusterPortRect_, 1, &clusterPortInput_))
      return true;
    if (hitField(clusterLoginRect_, 2, &clusterLoginInput_))
      return true;
    if (clusterWSJTX_ && wsjtxPortRect_.w > 0) {
      if (hitField(wsjtxPortRect_, 3, &wsjtxPortInput_))
        return true;
    }
  } else if (activeTab_ == Tab::Rig) {
    if (hitField(rigHostRect_, 0, &rigHostInput_))
      return true;
    if (hitField(rigPortRect_, 1, &rigPortInput_))
      return true;
    if (hitField(rotatorHostRect_, 2, &rotatorHostInput_))
      return true;
    if (hitField(rotatorPortRect_, 3, &rotatorPortInput_))
      return true;

    // Toggles
    if (mx >= toggleRect_.x && mx < toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my < toggleRect_.y + toggleRect_.h) {
      rigAutoTune_ = !rigAutoTune_;
      return true;
    }
    if (mx >= rotatorAutoTrackRect_.x &&
        mx < rotatorAutoTrackRect_.x + rotatorAutoTrackRect_.w &&
        my >= rotatorAutoTrackRect_.y &&
        my < rotatorAutoTrackRect_.y + rotatorAutoTrackRect_.h) {
      rotatorAutoTrack_ = !rotatorAutoTrack_;
      return true;
    }
    if (mx >= rotatorUpoverRect_.x &&
        mx < rotatorUpoverRect_.x + rotatorUpoverRect_.w &&
        my >= rotatorUpoverRect_.y &&
        my < rotatorUpoverRect_.y + rotatorUpoverRect_.h) {
      rotatorUpover_ = !rotatorUpover_;
      return true;
    }
  } else if (activeTab_ == Tab::Services) {
    if (hitField(qrzUsernameRect_, 0, &qrzUsernameInput_))
      return true;
    if (hitField(qrzPasswordRect_, 1, &qrzPasswordInput_))
      return true;
    if (hitField(repeaterBookRect_, 2, &repeaterBookInput_))
      return true;
    if (hitField(winlinkRect_, 3, &winlinkInput_))
      return true;
  } else if (activeTab_ == Tab::Network && hubMode_ == HubMode::Client) {
    if (hitField(hubIpRect_, 0, &hubIpInput_))
      return true;
    if (hitField(hubPortRect_, 1, &hubPortInput_))
      return true;
  } else if (activeTab_ == Tab::Appearance) {
    if (hitField(dimTimeRect_, 0, &dimTimeInput_))
      return true;
    if (hitField(brightTimeRect_, 1, &brightTimeInput_))
      return true;
  } else if (activeTab_ == Tab::Widgets) {
    // 1. Pane Switching (4 top-bar panes, 1 row)
    int yW = modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad +
             fieldH + cat->ptSize(FontStyle::SmallBold) + pad / 2;
    const int btnH = 22;
    int paneW = fieldW / 4;
    for (int i = 0; i < 4; ++i) {
      SDL_Rect pr = {fieldX + i * paneW, yW, paneW - 4, btnH};
      if (mx >= pr.x && mx < pr.x + pr.w && my >= pr.y && my < pr.y + pr.h) {
        if (activePane_ != i) {
          activePane_ = i;
          widgetListScrollOffset_ = 0;
        }
        return true;
      }
    }

    // 2. Sync Rotation Toggle
    if (mx >= syncRotationRect_.x &&
        mx < syncRotationRect_.x + syncRotationRect_.w &&
        my >= syncRotationRect_.y &&
        my < syncRotationRect_.y + syncRotationRect_.h) {
      syncRotation_ = !syncRotation_;
      return true;
    }

    // 3. Widget Selection (only within visible list area)
    if (my >= widgetListStartY_ && my < widgetListEndY_) {
      for (auto &wr : widgetRects_) {
        if (mx >= wr.rect.x && mx < wr.rect.x + wr.rect.w && my >= wr.rect.y &&
            my < wr.rect.y + wr.rect.h) {
          auto &cur = paneRotations_[activePane_];
          auto it = std::find(cur.begin(), cur.end(), wr.type);
          if (it != cur.end()) {
            if (cur.size() > 1)
              cur.erase(it);
          } else {
            cur.push_back(wr.type);
          }
          return true;
        }
      }
    }

    // 4. Rotation Interval
    if (mx >= rotationToggleRect_.x &&
        mx < rotationToggleRect_.x + rotationToggleRect_.w &&
        my >= rotationToggleRect_.y &&
        my < rotationToggleRect_.y + rotationToggleRect_.h) {
      activeField_ = 0; // Focus rotation interval input
      return true;
    }

    // 5. Side Panel Mode
    static const WidgetType kMode5[] = {
        WidgetType::DE_INFO, WidgetType::DX_CLUSTER, WidgetType::ON_THE_AIR,
        WidgetType::LIVE_SPOTS};
    for (int i = 0; i < 4; ++i) {
      auto &sr = sidePanelModeRects_[i];
      if (sr.w > 0 && mx >= sr.x && mx < sr.x + sr.w && my >= sr.y &&
          my < sr.y + sr.h) {
        paneRotations_[4] = {kMode5[i]};
        paneRotations_[5] = (i == 0)
                                ? std::vector<WidgetType>{WidgetType::DX_INFO}
                                : std::vector<WidgetType>{};
        return true;
      }
    }
  } else if (activeTab_ == Tab::Watchlist) {
    // Input field focus
    if (hitField(watchlistInputRect_, 0, &watchlistInputField_))
      return true;

    // Add button
    if (watchlistAddRect_.w > 0 &&
        mx >= watchlistAddRect_.x && mx < watchlistAddRect_.x + watchlistAddRect_.w &&
        my >= watchlistAddRect_.y && my < watchlistAddRect_.y + watchlistAddRect_.h) {
      std::string call = watchlistInputField_.getValue();
      if (!call.empty()) {
        std::transform(call.begin(), call.end(), call.begin(), ::toupper);
        if (std::find(watchlistEntries_.begin(), watchlistEntries_.end(), call) ==
            watchlistEntries_.end()) {
          watchlistEntries_.push_back(call);
        }
        watchlistInputField_.clear();
      }
      return true;
    }

    // Delete buttons
    for (int i = 0; i < (int)watchlistDeleteRects_.size(); ++i) {
      const SDL_Rect &dr = watchlistDeleteRects_[i];
      if (mx >= dr.x && mx < dr.x + dr.w && my >= dr.y && my < dr.y + dr.h) {
        int realIdx = watchlistScrollOffset_ + i;
        if (realIdx >= 0 && realIdx < (int)watchlistEntries_.size()) {
          watchlistEntries_.erase(watchlistEntries_.begin() + realIdx);
          if (watchlistScrollOffset_ > 0 &&
              watchlistScrollOffset_ >= (int)watchlistEntries_.size())
            --watchlistScrollOffset_;
        }
        return true;
      }
    }

    // Scroll arrows
    if (watchlistScrollUpRect_.w > 0 &&
        mx >= watchlistScrollUpRect_.x &&
        mx < watchlistScrollUpRect_.x + watchlistScrollUpRect_.w &&
        my >= watchlistScrollUpRect_.y &&
        my < watchlistScrollUpRect_.y + watchlistScrollUpRect_.h) {
      if (watchlistScrollOffset_ > 0)
        --watchlistScrollOffset_;
      return true;
    }
    if (watchlistScrollDownRect_.w > 0 &&
        mx >= watchlistScrollDownRect_.x &&
        mx < watchlistScrollDownRect_.x + watchlistScrollDownRect_.w &&
        my >= watchlistScrollDownRect_.y &&
        my < watchlistScrollDownRect_.y + watchlistScrollDownRect_.h) {
      int maxVisible = 8; // matches renderTabWatchlist
      if (watchlistScrollOffset_ + maxVisible < (int)watchlistEntries_.size())
        ++watchlistScrollOffset_;
      return true;
    }
  }

  // 3. Toggles and Buttons (Non-text fields)
  if (activeTab_ == Tab::Identity) {
    if (mx >= gpsToggleRect_.x && mx <= gpsToggleRect_.x + gpsToggleRect_.w &&
        my >= gpsToggleRect_.y && my <= gpsToggleRect_.y + gpsToggleRect_.h) {
      gpsEnabled_ = !gpsEnabled_;
      return true;
    }
  } else if (activeTab_ == Tab::Spotting) {
    if (mx >= clusterToggleRect_.x &&
        mx <= clusterToggleRect_.x + clusterToggleRect_.w &&
        my >= clusterToggleRect_.y &&
        my <= clusterToggleRect_.y + clusterToggleRect_.h) {
      clusterEnabled_ = !clusterEnabled_;
      return true;
    }
    if (mx >= toggleRect_.x && mx <= toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my <= toggleRect_.y + toggleRect_.h) {
      clusterWSJTX_ = !clusterWSJTX_;
      return true;
    }
    if (mx >= rbnToggleRect_.x && mx <= rbnToggleRect_.x + rbnToggleRect_.w &&
        my >= rbnToggleRect_.y && my <= rbnToggleRect_.y + rbnToggleRect_.h) {
      rbnEnabled_ = !rbnEnabled_;
      return true;
    }
  } else if (activeTab_ == Tab::Network) {
    if (mx >= hubModeRect_.x && mx <= hubModeRect_.x + hubModeRect_.w &&
        my >= hubModeRect_.y && my <= hubModeRect_.y + hubModeRect_.h) {
      if (hubMode_ == HubMode::Off)
        hubMode_ = HubMode::Master;
      else if (hubMode_ == HubMode::Master)
        hubMode_ = HubMode::Client;
      else
        hubMode_ = HubMode::Off;
      activeField_ = 0;
      return true;
    }
  } else if (activeTab_ == Tab::Appearance) {
    if (mx >= themeRect_.x && mx < themeRect_.x + themeRect_.w &&
        my >= themeRect_.y && my < themeRect_.y + themeRect_.h) {
      if (theme_ == "default")
        theme_ = "dark";
      else if (theme_ == "dark")
        theme_ = "glass";
      else if (theme_ == "glass")
        theme_ = "midnight";
      else if (theme_ == "midnight")
        theme_ = "amber";
      else if (theme_ == "amber")
        theme_ = "paper";
      else if (theme_ == "paper")
        theme_ = "matrix";
      else
        theme_ = "default";
      return true;
    }
    if (mx >= customizeBtnRect_.x &&
        mx < customizeBtnRect_.x + customizeBtnRect_.w &&
        my >= customizeBtnRect_.y &&
        my < customizeBtnRect_.y + customizeBtnRect_.h) {
      themeCustomizer_->setActive(true);
      return true;
    }
    if (mx >= nightLightsRect_.x &&
        mx < nightLightsRect_.x + nightLightsRect_.w &&
        my >= nightLightsRect_.y &&
        my < nightLightsRect_.y + nightLightsRect_.h) {
      mapNightLights_ = !mapNightLights_;
      return true;
    }
    if (mx >= metricToggleRect_.x &&
        mx < metricToggleRect_.x + metricToggleRect_.w &&
        my >= metricToggleRect_.y &&
        my < metricToggleRect_.y + metricToggleRect_.h) {
      useMetric_ = !useMetric_;
      return true;
    }
    if (mx >= brightnessSliderRect_.x &&
        mx < brightnessSliderRect_.x + brightnessSliderRect_.w &&
        my >= brightnessSliderRect_.y &&
        my < brightnessSliderRect_.y + brightnessSliderRect_.h) {
      int val = (mx - brightnessSliderRect_.x) * 100 / brightnessSliderRect_.w;
      brightnessMgr_.setBrightness(val);
      return true;
    }
    if (mx >= scheduleToggleRect_.x &&
        mx < scheduleToggleRect_.x + scheduleToggleRect_.w &&
        my >= scheduleToggleRect_.y &&
        my < scheduleToggleRect_.y + scheduleToggleRect_.h) {
      brightnessMgr_.setScheduleEnabled(!brightnessMgr_.isScheduleEnabled());
      return true;
    }
    if (mx >= powerMethodRect_.x && mx < powerMethodRect_.x + powerMethodRect_.w &&
        my >= powerMethodRect_.y && my < powerMethodRect_.y + powerMethodRect_.h) {
      std::vector<DisplayPower::Method> methods = {DisplayPower::Method::NONE};
      if (displayPower_) {
        const auto &available = displayPower_->getMethods();
        methods.insert(methods.end(), available.begin(), available.end());
      }
      
      auto current = DisplayPower::stringToMethod(displayPowerMethod_);
      auto it = std::find(methods.begin(), methods.end(), current);
      size_t idx = (it == methods.end()) ? 0 : std::distance(methods.begin(), it);
      idx = (idx + 1) % methods.size();
      displayPowerMethod_ = DisplayPower::methodToString(methods[idx]);
      return true;
    }
  } else if (activeTab_ == Tab::Rig) {
    if (mx >= toggleRect_.x && mx < toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my < toggleRect_.y + toggleRect_.h) {
      rigAutoTune_ = !rigAutoTune_;
      return true;
    }
    if (mx >= rotatorAutoTrackRect_.x &&
        mx < rotatorAutoTrackRect_.x + rotatorAutoTrackRect_.w &&
        my >= rotatorAutoTrackRect_.y &&
        my < rotatorAutoTrackRect_.y + rotatorAutoTrackRect_.h) {
      rotatorAutoTrack_ = !rotatorAutoTrack_;
      return true;
    }
    if (mx >= rotatorUpoverRect_.x &&
        mx < rotatorUpoverRect_.x + rotatorUpoverRect_.w &&
        my >= rotatorUpoverRect_.y &&
        my < rotatorUpoverRect_.y + rotatorUpoverRect_.h) {
      rotatorUpover_ = !rotatorUpover_;
      return true;
    }
  }

  // 4. Footer Buttons
  if (mx >= cancelBtnRect_.x && mx <= cancelBtnRect_.x + cancelBtnRect_.w &&
      my >= cancelBtnRect_.y && my <= cancelBtnRect_.y + cancelBtnRect_.h) {
    complete_ = true;
    cancelled_ = true;
    return true;
  }
  if (mx >= okBtnRect_.x && mx <= okBtnRect_.x + okBtnRect_.w &&
      my >= okBtnRect_.y && my <= okBtnRect_.y + okBtnRect_.h) {
    if (!callsignInput_.getValue().empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  }

  return false;
}

bool SetupScreen::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    return themeCustomizer_->onMouseUp(mx, my, mod, clicks);
  }

  // End any drag selection in the active text field
  if (TextInput *ti = getActiveInput())
    ti->onMouseUp();

  // If clicked outside modal, cancel
  if (mx < modalRect_.x || mx >= modalRect_.x + modalRect_.w ||
      my < modalRect_.y || my >= modalRect_.y + modalRect_.h) {
    cancelled_ = true;
    complete_ = true;
    return true;
  }

  return true;
}

bool SetupScreen::onMouseWheel(int scrollY) {
  if (activeTab_ == Tab::Widgets && widgetListMaxScroll_ > 0) {
    widgetListScrollOffset_ = std::max(
        0, std::min(widgetListScrollOffset_ - scrollY, widgetListMaxScroll_));
    return true;
  }
  return false;
}

void SetupScreen::onMouseMove(int mx, int /*my*/) {
  TextInput *ti = getActiveInput();
  if (!ti)
    return;

  // Resolve the stored rect for the active field so we can pass field bounds
  SDL_Rect r = {0, 0, 0, 0};
  if (activeTab_ == Tab::Identity) {
    const SDL_Rect *rects[] = {&callsignRect_, &gridRect_, &latRect_,
                               &lonRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Spotting) {
    const SDL_Rect *rects[] = {&clusterHostRect_, &clusterPortRect_,
                               &clusterLoginRect_, &wsjtxPortRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Rig) {
    const SDL_Rect *rects[] = {&rigHostRect_, &rigPortRect_, &rotatorHostRect_,
                               &rotatorPortRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Services) {
    // Both Username and Password use full fieldW in render pass
    int pad = 20;
    int fieldW = modalRect_.w - 2 * pad;
    int fieldX = modalRect_.x + pad;
    r = {fieldX, 0, fieldW, 0}; // Y and H not strictly needed for onMouseMove
  } else if (activeTab_ == Tab::Network) {
    const SDL_Rect *rects[] = {&hubIpRect_, &hubPortRect_};
    if (activeField_ >= 0 && activeField_ < 2)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Appearance) {
    const SDL_Rect *rects[] = {&dimTimeRect_, &brightTimeRect_};
    if (activeField_ == 0 || activeField_ == 1)
      r = *rects[activeField_];
  }

  if (r.w > 0) {
    int textPad = 7;
    ti->onMouseMove(mx, fontMgr_, r.x, r.w, FontStyle::SmallRegular, textPad);
  }
}

bool SetupScreen::onKeyDown(SDL_Keycode key, Uint16 mod) {
  int nFields = 1;

  if (activeTab_ == Tab::Identity) {
    nFields = 4;
  } else if (activeTab_ == Tab::Spotting) {
    nFields = clusterWSJTX_ ? 4 : 3;
  } else if (activeTab_ == Tab::Appearance) {
    nFields = 2; // 0=dimTime, 1=brightTime
  } else if (activeTab_ == Tab::Services) {
    nFields = 4;
  } else if (activeTab_ == Tab::Rig) {
    nFields = 4;
  } else if (activeTab_ == Tab::Widgets) {
    nFields = 1; // 0=rotation interval
  } else if (activeTab_ == Tab::Watchlist) {
    nFields = 1;
  }

  // Watchlist: Enter adds the entry
  if (activeTab_ == Tab::Watchlist && activeField_ == 0 &&
      (key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
    std::string call = watchlistInputField_.getValue();
    if (!call.empty()) {
      std::transform(call.begin(), call.end(), call.begin(), ::toupper);
      if (std::find(watchlistEntries_.begin(), watchlistEntries_.end(), call) ==
          watchlistEntries_.end()) {
        watchlistEntries_.push_back(call);
      }
      watchlistInputField_.clear();
    }
    return true;
  }

  switch (key) {
  case SDLK_ESCAPE:
    complete_ = true;
    cancelled_ = true;
    return true;
  case SDLK_TAB: {
    activeField_ = (activeField_ + 1) % nFields;
    TextInput *ni = getActiveInput();
    if (ni)
      ni->setCursorToEnd();
    return true;
  }
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    if (!callsignInput_.getValue().empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  case SDLK_BACKSPACE:
  case SDLK_DELETE: {
    bool isLatLon = (activeTab_ == Tab::Identity &&
                     (activeField_ == 2 || activeField_ == 3));
    bool isRotInterval = (activeTab_ == Tab::Widgets && activeField_ == 0);
    if (isRotInterval) {
      if (key == SDLK_BACKSPACE)
        rotationInterval_ /= 10;
      return true;
    }
    TextInput *ti = getActiveInput();
    if (ti) {
      bool hadContent = ti->hasSelection() ||
                        (key == SDLK_BACKSPACE
                             ? ti->getCursorPos() > 0
                             : ti->getCursorPos() < (int)ti->getValue().size());
      ti->onKeyDown(key, mod);
      if (hadContent && isLatLon)
        latLonManual_ = true;
    }
    return true;
  }
  case SDLK_v:
    if (mod & (KMOD_CTRL | KMOD_GUI)) {
      char *clip = SDL_GetClipboardText();
      if (clip) {
        for (char c : std::string(clip))
          onTextInput(std::string(1, c).c_str());
        SDL_free(clip);
      }
      return true;
    }
    // fall through to default for non-ctrl v
    {
      TextInput *ti = getActiveInput();
      if (ti)
        ti->onKeyDown(key, mod);
    }
    return true;
  default: {
    TextInput *ti = getActiveInput();
    if (ti)
      ti->onKeyDown(key, mod);
    return true;
  }
  }
}

bool SetupScreen::onTextInput(const char *inputText) {
  if (activeTab_ == Tab::Widgets) {
    if (activeField_ == 0) {
      // Rotation interval: numeric only
      if (inputText[0] >= '0' && inputText[0] <= '9') {
        rotationInterval_ = rotationInterval_ * 10 + (inputText[0] - '0');
        if (rotationInterval_ > 3600)
          rotationInterval_ = 3600;
      }
      return true;
    }
  }

  if (activeTab_ == Tab::Appearance) {
    // Fields 0 & 1: dimTime / brightTime (HH:MM) - auto-insert colon
    TextInput *ti = getActiveInput();
    if (ti && ti->getValue().size() == 2 && inputText[0] != ':') {
      ti->onTextInput(":");
    }
    if (ti)
      ti->onTextInput(inputText);
    return true;
  }

  if (activeTab_ == Tab::Watchlist && activeField_ == 0) {
    // Callsign: alphanumeric + slash only, auto-uppercase, max 12 chars
    for (const char *p = inputText; *p; ++p) {
      if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/')) {
        return true;
      }
    }
    if ((int)watchlistInputField_.getValue().size() < 12) {
      std::string upper = inputText;
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      watchlistInputField_.onTextInput(upper.c_str());
    }
    return true;
  }

  // === CALLSIGN VALIDATION (Identity Tab, Field 0) ===
  if (activeTab_ == Tab::Identity && activeField_ == 0) {
    if (callsignInput_.hasSelection()) {
      // If we have a selection, we're replacing it - just pass it through
      // and TextInput will handle upper/lower as needed via its own
      // render/storage but here we still want to force upper case for
      // callsigns.
      std::string upper = inputText;
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      callsignInput_.onTextInput(upper.c_str());
      return true;
    }
    for (const char *p = inputText; *p; ++p) {
      if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/')) {
        return true;
      }
    }
    std::string upper = inputText;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    callsignInput_.onTextInput(upper.c_str());
    return true;
  }

  // === GRID SQUARE VALIDATION (Identity Tab, Field 1) ===
  if (activeTab_ == Tab::Identity && activeField_ == 1) {
    if (gridInput_.hasSelection()) {
      gridInput_.onTextInput(inputText); // Let it through
      autoPopulateLatLon();
      return true;
    }
    const std::string &cur = gridInput_.getValue();
    for (const char *p = inputText; *p; ++p) {
      int pos = static_cast<int>(cur.size());
      if (pos >= 6) {
        return true;
      }
      if (pos < 2) {
        if (!((*p >= 'A' && *p <= 'R') || (*p >= 'a' && *p <= 'r'))) {
          return true;
        }
      } else if (pos < 4) {
        if (!(*p >= '0' && *p <= '9')) {
          return true;
        }
      } else {
        if (!((*p >= 'A' && *p <= 'X') || (*p >= 'a' && *p <= 'x'))) {
          return true;
        }
      }
    }
    std::string formatted = inputText;
    for (size_t i = 0; i < formatted.size(); ++i) {
      int pos =
          static_cast<int>(gridInput_.getValue().size()) + static_cast<int>(i);
      if (pos < 2) {
        formatted[i] = std::toupper(formatted[i]);
      } else if (pos >= 4) {
        formatted[i] = std::tolower(formatted[i]);
      }
    }
    gridInput_.onTextInput(formatted.c_str());
    latLonManual_ = false;
    return true;
  }

  // === LAT/LON VALIDATION (Identity Tab, Fields 2 & 3) ===
  if (activeTab_ == Tab::Identity && (activeField_ == 2 || activeField_ == 3)) {
    for (const char *p = inputText; *p; ++p) {
      if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
        continue;
      return true;
    }
    latLonManual_ = true;
    TextInput *ti = getActiveInput();
    if (ti)
      ti->onTextInput(inputText);
    return true;
  }

  // === PORT VALIDATION (Spotting Tab, Fields 1 and 3) ===
  if (activeTab_ == Tab::Spotting && (activeField_ == 1 || activeField_ == 3)) {
    for (const char *p = inputText; *p; ++p) {
      if (!(*p >= '0' && *p <= '9')) {
        return true;
      }
    }
    TextInput *ti = getActiveInput();
    if (ti) {
      std::string testValue = ti->getValue();
      testValue += inputText;
      int port = std::atoi(testValue.c_str());
      if (port > 65535 || port == 0) {
        return true;
      }
      ti->onTextInput(inputText);
    }
    return true;
  }

  // === DEFAULT INSERTION ===
  TextInput *ti = getActiveInput();
  if (ti)
    ti->onTextInput(inputText);

  return true;
}

void SetupScreen::renderTabWatchlist(SDL_Renderer *renderer, int /*cx*/,
                                     int pad, int fieldW, int fieldH,
                                     int fieldX, int textPad) {
  auto *cat = fontMgr_.catalog();
  int y = modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color red = {200, 60, 60, 255};

  cat->drawText(renderer, "Highlight Callsigns (Watchlist):", fieldX, y, orange,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad / 2;

  // List area: up to 8 visible rows
  const int maxVisible = 8;
  const int rowH = fieldH + 4;
  const int delBtnW = 36;

  watchlistDeleteRects_.clear();
  int visCount = std::min(maxVisible, (int)watchlistEntries_.size() -
                                          watchlistScrollOffset_);
  visCount = std::max(0, visCount);

  for (int i = 0; i < visCount; ++i) {
    int realIdx = watchlistScrollOffset_ + i;
    const std::string &call = watchlistEntries_[realIdx];

    // Row background
    SDL_Rect rowR = {fieldX, y, fieldW, fieldH};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderFillRect(renderer, &rowR);
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderDrawRect(renderer, &rowR);

    // Callsign text
    cat->drawText(renderer, call.c_str(), fieldX + textPad, y + fieldH / 2,
                  white, FontStyle::SmallRegular, false, false, true);

    // [X] delete button
    SDL_Rect delR = {fieldX + fieldW - delBtnW, y + 2, delBtnW, fieldH - 4};
    SDL_SetRenderDrawColor(renderer, 80, 30, 30, 255);
    SDL_RenderFillRect(renderer, &delR);
    SDL_SetRenderDrawColor(renderer, 140, 60, 60, 255);
    SDL_RenderDrawRect(renderer, &delR);
    cat->drawText(renderer, "X", delR.x + delR.w / 2, delR.y + delR.h / 2, red,
                  FontStyle::SmallBold, true, false, true);
    watchlistDeleteRects_.push_back(delR);

    y += rowH;
  }

  // Scroll arrows (shown when list is longer than maxVisible)
  watchlistScrollUpRect_ = {0, 0, 0, 0};
  watchlistScrollDownRect_ = {0, 0, 0, 0};
  if ((int)watchlistEntries_.size() > maxVisible) {
    SDL_Color arrowCol = {180, 180, 180, 255};
    if (watchlistScrollOffset_ > 0) {
      SDL_Rect upR = {fieldX, y, 40, fieldH};
      watchlistScrollUpRect_ = upR;
      SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
      SDL_RenderFillRect(renderer, &upR);
      SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
      SDL_RenderDrawRect(renderer, &upR);
      cat->drawText(renderer, "^", upR.x + upR.w / 2, upR.y + upR.h / 2,
                    arrowCol, FontStyle::Fast, true, false, true);
    }
    if (watchlistScrollOffset_ + maxVisible < (int)watchlistEntries_.size()) {
      SDL_Rect downR = {fieldX + 44, y, 40, fieldH};
      watchlistScrollDownRect_ = downR;
      SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
      SDL_RenderFillRect(renderer, &downR);
      SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
      SDL_RenderDrawRect(renderer, &downR);
      cat->drawText(renderer, "v", downR.x + downR.w / 2, downR.y + downR.h / 2,
                    arrowCol, FontStyle::Fast, true, false, true);
    }
    y += rowH;
  }

  y += pad / 2;

  // Count hint
  char hint[48];
  std::snprintf(hint, sizeof(hint), "%d callsign(s)",
                (int)watchlistEntries_.size());
  cat->drawText(renderer, hint, fieldX, y, gray, FontStyle::Fast);
  y += cat->ptSize(FontStyle::Fast) + pad / 2;

  // Input field + Add button
  const int addBtnW = 60;
  int inputW = fieldW - addBtnW - pad / 2;

  cat->drawText(renderer, "Add:", fieldX, y, white, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  watchlistInputRect_ = {fieldX, y, inputW, fieldH};
  watchlistAddRect_ = {fieldX + inputW + pad / 2, y, addBtnW, fieldH};

  watchlistInputField_.render(
      renderer, fontMgr_, fieldX, y, inputW, fieldH, FontStyle::SmallRegular,
      textPad, activeTab_ == Tab::Watchlist && activeField_ == 0, false, orange,
      gray, white, white, gray, "Enter callsign...");
  y += fieldH;

  SDL_SetRenderDrawColor(renderer, 40, 80, 40, 255);
  SDL_RenderFillRect(renderer, &watchlistAddRect_);
  SDL_SetRenderDrawColor(renderer, 80, 140, 80, 255);
  SDL_RenderDrawRect(renderer, &watchlistAddRect_);
  cat->drawText(renderer, "Add", watchlistAddRect_.x + watchlistAddRect_.w / 2,
                watchlistAddRect_.y + watchlistAddRect_.h / 2, white,
                FontStyle::SmallBold, true, false, true);
}

void SetupScreen::setConfig(const AppConfig &cfg) {
  // Set max lengths
  callsignInput_.setMaxLength(12);
  gridInput_.setMaxLength(6);
  latInput_.setMaxLength(12);
  lonInput_.setMaxLength(12);
  clusterHostInput_.setMaxLength(64);
  clusterPortInput_.setMaxLength(5);
  clusterLoginInput_.setMaxLength(12);
  wsjtxPortInput_.setMaxLength(5);
  qrzUsernameInput_.setMaxLength(32);
  qrzPasswordInput_.setMaxLength(32);
  dimTimeInput_.setMaxLength(5);
  brightTimeInput_.setMaxLength(5);
  rigHostInput_.setMaxLength(64);
  rigPortInput_.setMaxLength(5);
  hubIpInput_.setMaxLength(40);
  hubPortInput_.setMaxLength(5);
  watchlistInputField_.setMaxLength(12);

  gpsEnabled_ = cfg.gpsEnabled;
  callsignInput_.setValue(cfg.callsign);
  gridInput_.setValue(cfg.grid);
  frnText_ = cfg.callsignFrn;
  if (cfg.lat != 0.0 || cfg.lon != 0.0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lat);
    latInput_.setValue(buf);
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lon);
    lonInput_.setValue(buf);
  }
  clusterHostInput_.setValue(cfg.dxClusterHost);
  clusterPortInput_.setValue(std::to_string(cfg.dxClusterPort));
  clusterLoginInput_.setValue(cfg.dxClusterLogin);
  clusterEnabled_ = cfg.dxClusterEnabled;
  clusterWSJTX_ = cfg.dxClusterUseWSJTX;
  wsjtxPortInput_.setValue(std::to_string(cfg.wsjtxPort));
  rbnEnabled_ = cfg.rbnEnabled;
  pskOfDe_ = cfg.liveSpotsOfDe;
  pskUseCall_ = cfg.liveSpotsUseCall;
  pskMaxAge_ = cfg.liveSpotsMaxAge;

  rotationInterval_ = cfg.rotationIntervalS;
  syncRotation_ = cfg.syncRotation;
  watchlistEntries_ = cfg.watchlist;
  watchlistInputField_.clear();
  watchlistScrollOffset_ = 0;
  theme_ = cfg.theme;
  mapStyle_ = cfg.mapStyle;
  projection_ = cfg.projection;
  displayPowerMethod_ = cfg.displayPowerMethod;
  mapNightLights_ = cfg.mapNightLights;
  useMetric_ = cfg.useMetric;
  callsignColor_ = cfg.callsignColor;
  panelMode_ = cfg.panelMode;
  selectedSatellite_ = cfg.selectedSatellite;
  rssEnabled_ = cfg.rssEnabled;
  showBorders_ = cfg.showBorders;
  weatherOverlay_ = cfg.weatherOverlay;

  qrzUsernameInput_.setValue(cfg.qrzUsername);
  qrzPasswordInput_.setValue(cfg.qrzPassword);
  repeaterBookInput_.setValue(cfg.repeaterBookKey);
  winlinkInput_.setValue(cfg.winlinkKey);
  countdownLabel_ = cfg.countdownLabel;
  countdownTime_ = cfg.countdownTime;
  brightnessMgr_.setBrightness(cfg.brightness);
  brightnessMgr_.setScheduleEnabled(cfg.brightnessSchedule);
  brightnessMgr_.setDimTime(cfg.dimHour, cfg.dimMinute);
  brightnessMgr_.setBrightTime(cfg.brightHour, cfg.brightMinute);

  char timeBuf[16];
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", cfg.dimHour,
                cfg.dimMinute);
  dimTimeInput_.setValue(timeBuf);
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", cfg.brightHour,
                cfg.brightMinute);
  brightTimeInput_.setValue(timeBuf);

  rigHostInput_.setValue(cfg.rigHost);
  rigPortInput_.setValue(std::to_string(cfg.rigPort));
  rigAutoTune_ = cfg.rigAutoTune;

  rotatorHostInput_.setValue(cfg.rotatorHost);
  rotatorPortInput_.setValue(std::to_string(cfg.rotatorPort));
  rotatorAutoTrack_ = cfg.rotatorAutoTrack;
  rotatorUpover_ = cfg.rotatorUpover;

  hubMode_ = cfg.hubMode;
  hubIpInput_.setValue(cfg.hubIp);
  hubPortInput_.setValue(std::to_string(cfg.hubPort));

  paneRotations_[0] = cfg.pane1Rotation;
  paneRotations_[1] = cfg.pane2Rotation;
  paneRotations_[2] = cfg.pane3Rotation;
  paneRotations_[3] = cfg.pane4Rotation;
  paneRotations_[4] = cfg.pane5Rotation;
  paneRotations_[5] = cfg.pane6Rotation;

  colorOverrides_ = cfg.colorOverrides;

  callsignInput_.setCursorToEnd();
}

AppConfig SetupScreen::getConfig() const {
  AppConfig cfg;
  cfg.gpsEnabled = gpsEnabled_;
  cfg.callsign = callsignInput_.getValue();
  cfg.grid = gridInput_.getValue();
  cfg.callsignFrn = frnText_;
  cfg.lat = std::atof(latInput_.getValue().c_str());
  cfg.lon = std::atof(lonInput_.getValue().c_str());
  cfg.dxClusterHost = clusterHostInput_.getValue();
  cfg.dxClusterPort = std::atoi(clusterPortInput_.getValue().c_str());
  if (cfg.dxClusterPort == 0)
    cfg.dxClusterPort = 7300;
  cfg.dxClusterLogin = clusterLoginInput_.getValue();
  cfg.dxClusterEnabled = clusterEnabled_;
  cfg.dxClusterUseWSJTX = clusterWSJTX_;
  cfg.wsjtxPort = std::atoi(wsjtxPortInput_.getValue().c_str());
  if (cfg.wsjtxPort == 0)
    cfg.wsjtxPort = 2237;
  cfg.rbnEnabled = rbnEnabled_;
  cfg.liveSpotsOfDe = pskOfDe_;
  cfg.liveSpotsUseCall = pskUseCall_;
  cfg.liveSpotsMaxAge = pskMaxAge_;

  cfg.rotationIntervalS = rotationInterval_;
  cfg.syncRotation = syncRotation_;
  cfg.watchlist = watchlistEntries_;
  cfg.theme = theme_;
  cfg.mapStyle = mapStyle_;
  cfg.projection = projection_;
  cfg.displayPowerMethod = displayPowerMethod_;
  cfg.mapNightLights = mapNightLights_;
  cfg.useMetric = useMetric_;
  cfg.rssEnabled = rssEnabled_;
  cfg.showBorders = showBorders_;
  cfg.weatherOverlay = weatherOverlay_;
  cfg.callsignColor = callsignColor_;
  cfg.panelMode = panelMode_;
  cfg.selectedSatellite = selectedSatellite_;

  cfg.qrzUsername = qrzUsernameInput_.getValue();
  cfg.qrzPassword = qrzPasswordInput_.getValue();
  cfg.repeaterBookKey = repeaterBookInput_.getValue();
  cfg.winlinkKey = winlinkInput_.getValue();
  cfg.countdownLabel = countdownLabel_;
  cfg.countdownTime = countdownTime_;
  cfg.brightness = brightnessMgr_.getBrightness();
  cfg.brightnessSchedule = brightnessMgr_.isScheduleEnabled();

  int dh, dm, bh, bm;
  if (std::sscanf(dimTimeInput_.getValue().c_str(), "%d:%d", &dh, &dm) == 2) {
    cfg.dimHour = dh;
    cfg.dimMinute = dm;
  }
  if (std::sscanf(brightTimeInput_.getValue().c_str(), "%d:%d", &bh, &bm) ==
      2) {
    cfg.brightHour = bh;
    cfg.brightMinute = bm;
  }

  cfg.pane1Rotation = paneRotations_[0];
  cfg.pane2Rotation = paneRotations_[1];
  cfg.pane3Rotation = paneRotations_[2];
  cfg.pane4Rotation = paneRotations_[3];
  cfg.pane5Rotation = paneRotations_[4];
  cfg.pane6Rotation = paneRotations_[5];

  cfg.rigHost = rigHostInput_.getValue();
  cfg.rigPort = std::atoi(rigPortInput_.getValue().c_str());
  if (cfg.rigPort == 0)
    cfg.rigPort = 4532;
  cfg.rigAutoTune = rigAutoTune_;

  cfg.rotatorHost = rotatorHostInput_.getValue();
  cfg.rotatorPort = std::atoi(rotatorPortInput_.getValue().c_str());
  if (cfg.rotatorPort == 0)
    cfg.rotatorPort = 4533;
  cfg.rotatorAutoTrack = rotatorAutoTrack_;
  cfg.rotatorUpover = rotatorUpover_;

  cfg.hubMode = hubMode_;
  cfg.hubIp = hubIpInput_.getValue();
  cfg.hubPort = std::atoi(hubPortInput_.getValue().c_str());
  if (cfg.hubPort == 0)
    cfg.hubPort = 8080;

  cfg.colorOverrides = colorOverrides_;

  return cfg;
}

std::vector<std::string> SetupScreen::getActions() const {
  return {"tab_identity",
          "tab_dxcluster",
          "tab_appearance",
          "tab_widgets",
          "field_0",
          "field_1",
          "field_2",
          "field_3",
          "toggle_night_lights",
          "toggle_metric",
          "toggle_rss",
          "toggle_borders",
          "toggle_weather_overlay",
          "toggle_map_style",
          "done",
          "cancel"};
}

SDL_Rect SetupScreen::getActionRect(const std::string &action) const {
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return {0, 0, 0, 0};
  int pad = 20;
  int fieldW = modalRect_.w - 2 * pad;
  int fieldX = modalRect_.x + pad;
  int fieldH = cat->ptSize(FontStyle::SmallRegular) + 14;
  int titleShift = cat->ptSize(FontStyle::MediumBold) + pad / 2;
  int tabY = modalRect_.y + titleShift + pad / 2;
  int numTabs = 9;
  int tabW = fieldW / numTabs;

  if (action == "tab_identity")
    return {fieldX, tabY, tabW, fieldH};
  if (action == "tab_dxcluster")
    return {fieldX + tabW, tabY, tabW, fieldH};
  if (action == "tab_appearance")
    return {fieldX + 2 * tabW, tabY, tabW, fieldH};
  if (action == "tab_display")
    return {fieldX + 3 * tabW, tabY, tabW, fieldH};
  if (action == "tab_rig")
    return {fieldX + 4 * tabW, tabY, tabW, fieldH};
  if (action == "tab_services")
    return {fieldX + 5 * tabW, tabY, tabW, fieldH};
  if (action == "tab_widgets")
    return {fieldX + 6 * tabW, tabY, tabW, fieldH};

  // Fields (approximate positions)
  int yStart = modalRect_.y + titleShift + 2 * pad + fieldH;
  if (action.find("field_") == 0) {
    int idx = StringUtils::safe_stoi(action.substr(6));
    int fy =
        yStart + idx * (cat->ptSize(FontStyle::SmallBold) + fieldH + pad / 2);
    return {fieldX, fy, fieldW, fieldH};
  }

  if (action == "toggle_night_lights") {
    return nightLightsRect_;
  }
  if (action == "toggle_metric")
    return metricToggleRect_;
  if (action == "toggle_rss")
    return rssToggleRect_;
  if (action == "toggle_borders")
    return bordersToggleRect_;
  if (action == "toggle_weather_overlay")
    return weatherOverlayRect_;
  if (action == "toggle_map_style")
    return mapStyleRect_;

  if (action == "done") {
    return okBtnRect_;
  }
  if (action == "cancel") {
    return cancelBtnRect_;
  }

  return {0, 0, 0, 0};
}
