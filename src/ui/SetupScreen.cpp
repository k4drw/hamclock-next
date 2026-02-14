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

  mismatchWarning_ = false;
  if (latLonManual_ && gridValid_ && !latText_.empty() && !lonText_.empty()) {
    double manLat = std::atof(latText_.c_str());
    double manLon = std::atof(lonText_.c_str());
    double tolLat = (gridText_.size() >= 6) ? 0.5 : 1.0;
    double tolLon = (gridText_.size() >= 6) ? 1.0 : 2.0;
    if (std::fabs(manLat - gridLat_) > tolLat ||
        std::fabs(manLon - gridLon_) > tolLon) {
      mismatchWarning_ = true;
    }
  }
}

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

  SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  int cx = x_ + width_ / 2;
  int pad = std::max(16, width_ / 24);
  int fieldW = std::min(400, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;
  int textPad = 7;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color cyan = {0, 200, 255, 255};
  SDL_Color gray = {120, 120, 120, 255};

  int y = y_ + pad;

  fontMgr_.drawText(renderer, "HamClock-Next Setup", cx, y, cyan, titleSize_,
                    true, true);
  y += titleSize_ + pad;

  const char *tabs[] = {"Identity", "DX Cluster", "Appearance", "Widgets"};
  int numTabs = 4;
  int tabW = fieldW / numTabs;
  for (int i = 0; i < numTabs; ++i) {
    SDL_Rect tr = {fieldX + i * tabW, y, tabW, fieldH};
    bool active = (int)activeTab_ == i;
    SDL_SetRenderDrawColor(renderer, active ? 40 : 20, active ? 40 : 25,
                           active ? 60 : 30, 255);
    SDL_RenderFillRect(renderer, &tr);
    SDL_SetRenderDrawColor(renderer, active ? 0 : 80, active ? 200 : 80,
                           active ? 255 : 80, 255);
    SDL_RenderDrawRect(renderer, &tr);
    fontMgr_.drawText(renderer, tabs[i], tr.x + tabW / 2, tr.y + fieldH / 2,
                      active ? white : gray, labelSize_, false, true);
  }
  y += fieldH + pad / 2;
  int contentY = y;

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
  case Tab::Widgets:
    renderTabWidgets(renderer, cx, pad, fieldW, fieldH, fieldX, textPad);
    break;
  }

  y = y_ + height_ - pad - 40;
  int btnW = 100;
  int btnH = 34;

  // Cancel Button
  SDL_Rect cancelBtn = {cx - btnW - 20, y, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 60, 20, 20, 255);
  SDL_RenderFillRect(renderer, &cancelBtn);
  SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
  SDL_RenderDrawRect(renderer, &cancelBtn);
  fontMgr_.drawText(renderer, "Cancel", cancelBtn.x + btnW / 2,
                    cancelBtn.y + btnH / 2, white, labelSize_, false, true);
  cancelBtnRect_ = cancelBtn;

  // Done Button
  SDL_Rect okBtn = {cx + 20, y, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255);
  SDL_RenderFillRect(renderer, &okBtn);
  SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
  SDL_RenderDrawRect(renderer, &okBtn);
  fontMgr_.drawText(renderer, "Done", okBtn.x + btnW / 2, okBtn.y + btnH / 2,
                    white, labelSize_, false, true);
  okBtnRect_ = okBtn;
}

void SetupScreen::renderTabIdentity(SDL_Renderer *renderer, int, int pad,
                                    int fieldW, int fieldH, int fieldX,
                                    int textPad) {
  int y = (y_ + titleSize_ + 2 * pad + fieldH + pad / 2);
  int vSpace = pad / 2;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color green = {0, 200, 0, 255};
  SDL_Color red = {255, 80, 80, 255};

  fontMgr_.drawText(renderer, "Callsign:", fieldX, y, white, labelSize_, true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, callsignText_, "e.g. K4DRW", fieldX, y,
              fieldW, fieldH, fieldSize_, textPad, activeField_ == 0,
              !callsignText_.empty(), cursorPos_, orange, gray, white, white,
              gray);
  y += vSpace;

  fontMgr_.drawText(renderer, "Grid Square:", fieldX, y, white, labelSize_,
                    true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, gridText_, "e.g. EL87qr", fieldX, y, fieldW,
              fieldH, fieldSize_, textPad, activeField_ == 1, gridValid_,
              cursorPos_, orange, gray, green, white, gray);
  y += vSpace;

  int halfFieldW = (fieldW - pad) / 2;
  fontMgr_.drawText(renderer, "Latitude:", fieldX, y, white, labelSize_, true);
  fontMgr_.drawText(renderer, "Longitude:", fieldX + halfFieldW + pad, y, white,
                    labelSize_, true);
  y += labelSize_ + 4;

  int latY = y;
  renderField(renderer, fontMgr_, latText_, "e.g. 27.76", fieldX, latY,
              halfFieldW, fieldH, fieldSize_, textPad, activeField_ == 2,
              !latText_.empty(), cursorPos_, orange, gray, white, white, gray);

  int lonY = y;
  renderField(renderer, fontMgr_, lonText_, "e.g. -82.64",
              fieldX + halfFieldW + pad, lonY, halfFieldW, fieldH, fieldSize_,
              textPad, activeField_ == 3, !lonText_.empty(), cursorPos_, orange,
              gray, white, white, gray);
  y = std::max(latY, lonY) + pad / 2;

  if (mismatchWarning_) {
    fontMgr_.drawText(renderer, "Warning: Lat/Lon outside grid square", fieldX,
                      y, red, hintSize_);
  } else if (gridValid_ && !latLonManual_) {
    fontMgr_.drawText(renderer, "Auto-calculated from grid", fieldX, y, gray,
                      hintSize_);
  }
}

void SetupScreen::renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad,
                                     int fieldW, int fieldH, int fieldX,
                                     int textPad) {
  int y = (y_ + titleSize_ + 2 * pad + fieldH + pad / 2);
  int vSpace = 5;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  // --- DX CLUSTER SECTION ---
  fontMgr_.drawText(renderer, "--- DX Cluster ---", cx, y, cyan, labelSize_,
                    true, true);
  y += labelSize_ + vSpace;

  fontMgr_.drawText(renderer, "Host:", fieldX, y, white, labelSize_, true);
  fontMgr_.drawText(renderer, "Port:", fieldX + fieldW / 2 + pad, y, white,
                    labelSize_, true);
  y += labelSize_ + 4;

  int halfW = (fieldW - pad) / 2;
  int hostY = y;
  renderField(renderer, fontMgr_, clusterHost_, "dxusa.net", fieldX, hostY,
              halfW, fieldH, fieldSize_, textPad, activeField_ == 0,
              !clusterHost_.empty(), cursorPos_, orange, gray, white, white,
              gray);
  int portY = y;
  renderField(renderer, fontMgr_, clusterPort_, "7300", fieldX + halfW + pad,
              portY, halfW, fieldH, fieldSize_, textPad, activeField_ == 1,
              !clusterPort_.empty(), cursorPos_, orange, gray, white, white,
              gray);
  y += fieldH + vSpace;

  fontMgr_.drawText(renderer, "Login:", fieldX, y, white, labelSize_, true);
  y += labelSize_ + 4;
  renderField(renderer, fontMgr_, clusterLogin_, "NOCALL", fieldX, y, fieldW,
              fieldH, fieldSize_, textPad, activeField_ == 2,
              !clusterLogin_.empty(), cursorPos_, orange, gray, white, white,
              gray);
  y += fieldH + vSpace;

  // Toggles row 1
  SDL_Rect enableToggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &enableToggle);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &enableToggle);
  if (clusterEnabled_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  fontMgr_.drawText(renderer, "Enable DX Cluster", fieldX + 30, y + 2, white,
                    labelSize_);
  clusterToggleRect_ = enableToggle;

  y += 24;

  SDL_Rect toggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &toggle);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &toggle);
  if (clusterWSJTX_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  fontMgr_.drawText(renderer, "Use WSJ-TX (UDP Port 2237)", fieldX + 30, y + 2,
                    white, labelSize_);
  toggleRect_ = toggle;
  y += 30;
}

void SetupScreen::renderTabAppearance(SDL_Renderer *renderer, int, int pad,
                                      int fieldW, int fieldH, int fieldX,
                                      int textPad) {
  int y = (y_ + titleSize_ + 2 * pad + fieldH + pad / 2);
  int vSpace = pad / 2;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color orange = {255, 165, 0, 255};
  SDL_Color gray = {140, 140, 140, 255};

  fontMgr_.drawText(renderer, "Pane Rotation Interval (seconds):", fieldX, y,
                    white, labelSize_, true);
  y += labelSize_ + 4;
  std::string rotStr = std::to_string(rotationInterval_);
  renderField(renderer, fontMgr_, rotStr, "30", fieldX, y, fieldW, fieldH,
              fieldSize_, textPad, activeField_ == 0, true, cursorPos_, orange,
              gray, white, white, gray);
  y += pad;

  fontMgr_.drawText(renderer, "Theme:", fieldX, y, white, labelSize_);
  SDL_Rect themeBtn = {fieldX + fieldW - 100, y, 100, 24};
  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &themeBtn);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &themeBtn);
  fontMgr_.drawText(renderer, theme_, themeBtn.x + themeBtn.w / 2,
                    themeBtn.y + themeBtn.h / 2, white, hintSize_, false, true);
  themeRect_ = themeBtn;
  y += vSpace * 2;

  SDL_Rect nlToggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &nlToggle);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &nlToggle);
  if (mapNightLights_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  fontMgr_.drawText(renderer, "Enable Map Night Lights", fieldX + 30, y + 2,
                    white, labelSize_);
  nightLightsRect_ = nlToggle;
  y += pad;

  SDL_Rect metricToggle = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &metricToggle);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &metricToggle);
  if (useMetric_) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  fontMgr_.drawText(renderer, "Use Metric Units (Celsius, km, m/s)",
                    fieldX + 30, y + 2, white, labelSize_);
  metricToggleRect_ = metricToggle;
  y += pad;

  fontMgr_.drawText(renderer, "Callsign Color:", fieldX, y, white, labelSize_);
  SDL_Rect colorBox = {fieldX + fieldW - 40, y, 40, 20};
  SDL_SetRenderDrawColor(renderer, callsignColor_.r, callsignColor_.g,
                         callsignColor_.b, 255);
  SDL_RenderFillRect(renderer, &colorBox);
  y += pad * 2;

  fontMgr_.drawText(renderer, "(Selection of colors coming soon...)", fieldX, y,
                    gray, hintSize_);
}

void SetupScreen::renderTabWidgets(SDL_Renderer *renderer, int cx, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int textPad) {
  int y = (y_ + titleSize_ + 2 * pad + fieldH + pad / 2);
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {140, 140, 140, 255};

  fontMgr_.drawText(renderer, "Select Active Widgets for Each Pane:", fieldX, y,
                    white, labelSize_, true);
  y += labelSize_ + pad / 2;

  int paneW = fieldW / 4;
  for (int i = 0; i < 4; ++i) {
    SDL_Rect pr = {fieldX + i * paneW, y, paneW - 4, 30};
    bool active = activePane_ == i;
    SDL_SetRenderDrawColor(renderer, active ? 60 : 30, active ? 60 : 30,
                           active ? 80 : 40, 255);
    SDL_RenderFillRect(renderer, &pr);
    SDL_SetRenderDrawColor(renderer, active ? 0 : 200, active ? 200 : 80,
                           active ? 255 : 80, 255);
    SDL_RenderDrawRect(renderer, &pr);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "Pane %d", i + 1);
    fontMgr_.drawText(renderer, buf, pr.x + pr.w / 2, pr.y + pr.h / 2,
                      active ? white : gray, hintSize_, false, true);
  }
  y += 35;

  widgetRects_.clear();
  int colW = fieldW / 2;
  int curX = fieldX;
  int startY = y;
  int rowH = hintSize_ + 6;

  WidgetType allTypes[] = {
      WidgetType::SOLAR,        WidgetType::DX_CLUSTER,
      WidgetType::LIVE_SPOTS,   WidgetType::BAND_CONDITIONS,
      WidgetType::CONTESTS,     WidgetType::ON_THE_AIR,
      WidgetType::GIMBAL,       WidgetType::MOON,
      WidgetType::CLOCK_AUX,    WidgetType::DX_PEDITIONS,
      WidgetType::DE_WEATHER,   WidgetType::DX_WEATHER,
      WidgetType::NCDXF,        WidgetType::SDO,
      WidgetType::HISTORY_FLUX, WidgetType::HISTORY_KP,
      WidgetType::HISTORY_SSN,  WidgetType::DRAP,
      WidgetType::AURORA,       WidgetType::ADIF,
      WidgetType::COUNTDOWN};

  const auto &currentPane = paneRotations_[activePane_];

  for (size_t i = 0; i < sizeof(allTypes) / sizeof(allTypes[0]); ++i) {
    WidgetType t = allTypes[i];
    SDL_Rect r = {curX, y, 16, 16};
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &r);

    bool selected = std::find(currentPane.begin(), currentPane.end(), t) !=
                    currentPane.end();
    if (selected) {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
      SDL_Rect check = {r.x + 3, r.y + 3, 10, 10};
      SDL_RenderFillRect(renderer, &check);
    }

    fontMgr_.drawText(renderer, widgetTypeDisplayName(t), r.x + 22, r.y, white,
                      hintSize_);

    widgetRects_.push_back({t, r});

    y += rowH;
    if (i == 10) {
      y = startY;
      curX += colW;
    }
  }
}

void SetupScreen::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool SetupScreen::onMouseUp(int mx, int my, Uint16) {
  int cx = x_ + width_ / 2;
  int pad = std::max(16, width_ / 24);
  int fieldW = std::min(400, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;
  int y = y_ + titleSize_ + 2 * pad;

  // Check Footer Buttons
  if (mx >= cancelBtnRect_.x && mx <= cancelBtnRect_.x + cancelBtnRect_.w &&
      my >= cancelBtnRect_.y && my <= cancelBtnRect_.y + cancelBtnRect_.h) {
    complete_ = true;
    cancelled_ = true;
    return true;
  }

  if (mx >= okBtnRect_.x && mx <= okBtnRect_.x + okBtnRect_.w &&
      my >= okBtnRect_.y && my <= okBtnRect_.y + okBtnRect_.h) {
    if (!callsignText_.empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  }

  int numTabs = 4;
  int tabW = fieldW / numTabs;
  if (my >= y && my <= y + fieldH) {
    for (int i = 0; i < numTabs; ++i) {
      if (mx >= fieldX + i * tabW && mx <= fieldX + (i + 1) * tabW) {
        activeTab_ = (Tab)i;
        activeField_ = 0;
        cursorPos_ = 0;
        return true;
      }
    }
  }

  if (activeTab_ == Tab::Spotting) {
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
  }

  if (activeTab_ == Tab::Appearance) {
    if (mx >= themeRect_.x && mx <= themeRect_.x + themeRect_.w &&
        my >= themeRect_.y && my <= themeRect_.y + themeRect_.h) {
      if (theme_ == "default")
        theme_ = "dark";
      else if (theme_ == "dark")
        theme_ = "glass";
      else
        theme_ = "default";
      return true;
    }
    if (mx >= nightLightsRect_.x &&
        mx <= nightLightsRect_.x + nightLightsRect_.w &&
        my >= nightLightsRect_.y &&
        my <= nightLightsRect_.y + nightLightsRect_.h) {
      mapNightLights_ = !mapNightLights_;
      return true;
    }
    if (mx >= metricToggleRect_.x &&
        mx <= metricToggleRect_.x + metricToggleRect_.w &&
        my >= metricToggleRect_.y &&
        my <= metricToggleRect_.y + metricToggleRect_.h) {
      useMetric_ = !useMetric_;
      return true;
    }
  }

  if (activeTab_ == Tab::Widgets) {
    int yTabBase = y_ + titleSize_ + 2 * pad + fieldH + pad / 2;
    int ySelector = yTabBase + labelSize_ + pad / 2;
    int paneW = fieldW / 4;
    if (my >= ySelector && my <= ySelector + 30) {
      for (int i = 0; i < 4; ++i) {
        if (mx >= fieldX + i * paneW && mx <= fieldX + (i + 1) * paneW) {
          activePane_ = i;
          return true;
        }
      }
    }

    // Check widget selection clicks
    for (const auto &wr : widgetRects_) {
      if (mx >= wr.rect.x && mx <= wr.rect.x + wr.rect.w && my >= wr.rect.y &&
          my <= wr.rect.y + wr.rect.h) {
        auto &v = paneRotations_[activePane_];
        auto it = std::find(v.begin(), v.end(), wr.type);
        if (it != v.end()) {
          v.erase(it);
        } else {
          v.push_back(wr.type);
        }
        return true;
      }
    }
  }

  // Handle generic field clicks for active tab
  int yStart = y_ + titleSize_ + 2 * pad + fieldH + pad / 2;
  int nFields = 0;
  if (activeTab_ == Tab::Identity)
    nFields = 4;
  else if (activeTab_ == Tab::Spotting)
    nFields = 3;
  else if (activeTab_ == Tab::Appearance)
    nFields = 1;

  for (int i = 0; i < nFields; ++i) {
    int fy = yStart;
    int fx = fieldX;
    int fw = fieldW;

    if (activeTab_ == Tab::Identity) {
      if (i < 2) {
        fy = yStart + i * (labelSize_ + 4 + fieldH + pad / 2);
      } else {
        fy = yStart + 2 * (labelSize_ + 4 + fieldH + pad / 2);
        int halfW = (fieldW - pad) / 2;
        if (i == 2) {
          fw = halfW;
        } else {
          fx = fieldX + halfW + pad;
          fw = halfW;
        }
      }
    } else {
      fy = yStart + i * (labelSize_ + 4 + fieldH + pad / 2);
    }

    if (mx >= fx && mx < fx + fw && my >= fy + labelSize_ &&
        my < fy + labelSize_ + fieldH) {
      activeField_ = i;
      cursorPos_ = 0;
      return true;
    }
  }

  return true;
}

bool SetupScreen::onKeyDown(SDL_Keycode key, Uint16) {
  std::string *text = nullptr;
  int nFields = 1;

  if (activeTab_ == Tab::Identity) {
    nFields = 4;
    switch (activeField_) {
    case 0:
      text = &callsignText_;
      break;
    case 1:
      text = &gridText_;
      break;
    case 2:
      text = &latText_;
      break;
    case 3:
      text = &lonText_;
      break;
    }
  } else if (activeTab_ == Tab::Spotting) {
    nFields = 3;
    switch (activeField_) {
    case 0:
      text = &clusterHost_;
      break;
    case 1:
      text = &clusterPort_;
      break;
    case 2:
      text = &clusterLogin_;
      break;
    }
  } else if (activeTab_ == Tab::Appearance) {
    nFields = 1;
  }

  switch (key) {
  case SDLK_ESCAPE:
    complete_ = true;
    cancelled_ = true;
    return true;
  case SDLK_TAB:
    activeField_ = (activeField_ + 1) % nFields;
    cursorPos_ = 0;
    return true;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    if (!callsignText_.empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  case SDLK_BACKSPACE:
    if (text && cursorPos_ > 0) {
      text->erase(cursorPos_ - 1, 1);
      --cursorPos_;
      if (activeTab_ == Tab::Identity &&
          (activeField_ == 2 || activeField_ == 3))
        latLonManual_ = true;
    } else if (activeTab_ == Tab::Appearance && activeField_ == 0) {
      rotationInterval_ /= 10;
    }
    return true;
  case SDLK_DELETE:
    if (text && cursorPos_ < static_cast<int>(text->size())) {
      text->erase(cursorPos_, 1);
      if (activeTab_ == Tab::Identity &&
          (activeField_ == 2 || activeField_ == 3))
        latLonManual_ = true;
    }
    return true;
  case SDLK_LEFT:
    if (cursorPos_ > 0)
      --cursorPos_;
    return true;
  case SDLK_RIGHT:
    if (text && cursorPos_ < static_cast<int>(text->size()))
      ++cursorPos_;
    return true;
  case SDLK_HOME:
    cursorPos_ = 0;
    return true;
  case SDLK_END:
    if (text)
      cursorPos_ = static_cast<int>(text->size());
    return true;
  default:
    return true;
  }
}

bool SetupScreen::onTextInput(const char *inputText) {
  std::string *field = nullptr;
  int maxLen = 12;

  if (activeTab_ == Tab::Identity) {
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
  } else if (activeTab_ == Tab::Spotting) {
    switch (activeField_) {
    case 0:
      field = &clusterHost_;
      maxLen = 64;
      break;
    case 1:
      field = &clusterPort_;
      maxLen = 5;
      break;
    case 2:
      field = &clusterLogin_;
      maxLen = 12;
      break;
    }
  } else if (activeTab_ == Tab::Appearance) {
    if (activeField_ == 0) {
      if (inputText[0] >= '0' && inputText[0] <= '9') {
        rotationInterval_ = rotationInterval_ * 10 + (inputText[0] - '0');
        if (rotationInterval_ > 3600)
          rotationInterval_ = 3600;
      }
      return true;
    }
  }

  if (!field)
    return true;
  if (static_cast<int>(field->size()) >= maxLen)
    return true;

  if (activeTab_ == Tab::Identity && (activeField_ == 2 || activeField_ == 3)) {
    for (const char *p = inputText; *p; ++p) {
      if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
        continue;
      return true;
    }
    latLonManual_ = true;
  }

  field->insert(cursorPos_, inputText);
  cursorPos_ += static_cast<int>(std::strlen(inputText));
  if (activeTab_ == Tab::Identity && activeField_ == 1)
    latLonManual_ = false;

  return true;
}

void SetupScreen::setConfig(const AppConfig &cfg) {
  callsignText_ = cfg.callsign;
  gridText_ = cfg.grid;
  if (cfg.lat != 0.0 || cfg.lon != 0.0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lat);
    latText_ = buf;
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lon);
    lonText_ = buf;
  }
  clusterHost_ = cfg.dxClusterHost;
  clusterPort_ = std::to_string(cfg.dxClusterPort);
  clusterLogin_ = cfg.dxClusterLogin;
  clusterEnabled_ = cfg.dxClusterEnabled;
  clusterWSJTX_ = cfg.dxClusterUseWSJTX;
  pskOfDe_ = cfg.pskOfDe;
  pskUseCall_ = cfg.pskUseCall;
  pskMaxAge_ = cfg.pskMaxAge;

  rotationInterval_ = cfg.rotationIntervalS;
  theme_ = cfg.theme;
  mapNightLights_ = cfg.mapNightLights;
  useMetric_ = cfg.useMetric;
  callsignColor_ = cfg.callsignColor;
  panelMode_ = cfg.panelMode;
  selectedSatellite_ = cfg.selectedSatellite;

  paneRotations_[0] = cfg.pane1Rotation;
  paneRotations_[1] = cfg.pane2Rotation;
  paneRotations_[2] = cfg.pane3Rotation;
  paneRotations_[3] = cfg.pane4Rotation;

  cursorPos_ = static_cast<int>(callsignText_.size());
}

AppConfig SetupScreen::getConfig() const {
  AppConfig cfg;
  cfg.callsign = callsignText_;
  cfg.grid = gridText_;
  cfg.lat = std::atof(latText_.c_str());
  cfg.lon = std::atof(lonText_.c_str());
  cfg.dxClusterHost = clusterHost_;
  cfg.dxClusterPort = std::atoi(clusterPort_.c_str());
  if (cfg.dxClusterPort == 0)
    cfg.dxClusterPort = 7300;
  cfg.dxClusterLogin = clusterLogin_;
  cfg.dxClusterEnabled = clusterEnabled_;
  cfg.dxClusterUseWSJTX = clusterWSJTX_;
  cfg.pskOfDe = pskOfDe_;
  cfg.pskUseCall = pskUseCall_;
  cfg.pskMaxAge = pskMaxAge_;

  cfg.rotationIntervalS = rotationInterval_;
  cfg.theme = theme_;
  cfg.mapNightLights = mapNightLights_;
  cfg.useMetric = useMetric_;
  cfg.callsignColor = callsignColor_;
  cfg.panelMode = panelMode_;
  cfg.selectedSatellite = selectedSatellite_;

  cfg.pane1Rotation = paneRotations_[0];
  cfg.pane2Rotation = paneRotations_[1];
  cfg.pane3Rotation = paneRotations_[2];
  cfg.pane4Rotation = paneRotations_[3];

  return cfg;
}

std::vector<std::string> SetupScreen::getActions() const {
  return {"tab_identity", "tab_dxcluster", "tab_appearance",
          "tab_widgets",  "field_0",       "field_1",
          "field_2",      "field_3",       "toggle_night_lights",
          "done",         "cancel"};
}

SDL_Rect SetupScreen::getActionRect(const std::string &action) const {
  int cx = x_ + width_ / 2;
  int pad = std::max(16, width_ / 24);
  int fieldW = std::min(400, width_ - 2 * pad);
  int fieldX = cx - fieldW / 2;
  int fieldH = fieldSize_ + 14;
  int tabY = y_ + titleSize_ + 2 * pad;
  int tabW = fieldW / 4;

  if (action == "tab_identity")
    return {fieldX, tabY, tabW, fieldH};
  if (action == "tab_dxcluster")
    return {fieldX + tabW, tabY, tabW, fieldH};
  if (action == "tab_appearance")
    return {fieldX + 2 * tabW, tabY, tabW, fieldH};
  if (action == "tab_widgets")
    return {fieldX + 3 * tabW, tabY, tabW, fieldH};

  // Fields (approximate positions)
  int yStart = y_ + titleSize_ + 3 * pad + fieldH;
  if (action.find("field_") == 0) {
    int idx = std::stoi(action.substr(6));
    int fy = yStart + idx * (labelSize_ + fieldH + pad / 2);
    return {fieldX, fy, fieldW, fieldH};
  }

  if (action == "toggle_night_lights") {
    return nightLightsRect_;
  }

  if (action == "done") {
    return okBtnRect_;
  }
  if (action == "cancel") {
    return cancelBtnRect_;
  }

  return {0, 0, 0, 0};
}
