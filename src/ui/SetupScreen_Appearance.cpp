#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>
#include <string>
#ifndef __EMSCRIPTEN__
#include <cstdlib>
#include <filesystem>
#endif

#ifndef __EMSCRIPTEN__
std::vector<std::pair<std::string, std::string>>
SetupScreen::enumerateSystemFonts() {
  std::vector<std::pair<std::string, std::string>> result;
  std::vector<std::string> searchDirs;
#if defined(_WIN32)
  // Windows system fonts
  const char *sysRoot = std::getenv("SystemRoot");
  if (sysRoot)
    searchDirs.push_back(std::string(sysRoot) + "\\Fonts\\");
  else
    searchDirs.push_back("C:\\Windows\\Fonts\\");
  // User-installed fonts (Windows 10+)
  const char *localAppData = std::getenv("LOCALAPPDATA");
  if (localAppData)
    searchDirs.push_back(std::string(localAppData) + "\\Microsoft\\Windows\\Fonts\\");
#else
  searchDirs.push_back("/usr/share/fonts/");
  searchDirs.push_back("/usr/local/share/fonts/");
  const char *home = std::getenv("HOME");
  if (home) {
    searchDirs.push_back(std::string(home) + "/.local/share/fonts/");
    searchDirs.push_back(std::string(home) + "/.fonts/");
  }
#endif
  for (const auto &dir : searchDirs) {
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
      continue;
    for (; it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
      if (ec) { ec.clear(); continue; }
      auto ext = it->path().extension().string();
      if (ext != ".ttf" && ext != ".otf")
        continue;
      std::string stem = it->path().stem().string();
      std::string fullPath = it->path().string();
      result.push_back({stem, fullPath});
    }
  }
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  return result;
}
#endif

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
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  // --- Appearance section ---
  cat->drawText(renderer, "--- Appearance ---", cx, y, themes.accent,
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
  cat->drawText(renderer, "Theme:", xRow1, y + 12, themes.text,
                FontStyle::SmallRegular, false, false, true);
  themeRect_ = {xRow1 + wThemeLabel + 10, y, 80, 24};
  customizeBtnRect_ = {themeRect_.x + 95, y, 85, 24};
  drawBtn(themeRect_, theme_);
  drawBtn(customizeBtnRect_, "Customize");
  y += 24 + vSpace;

  // Row 2: Toggles (Night Lights, Metric) - Centered
  auto drawToggle = [&](SDL_Rect &r, bool val, const char *lbl) {
    SDL_Rect box = {r.x, r.y, 20, 20};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &box);
    if (val) {
      SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
      SDL_Rect check = {r.x + 4, r.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, lbl, r.x + 25, r.y + box.h / 2, themes.text,
                  FontStyle::SmallRegular, false, false, true);
  };

  int wNL = 25 + fontMgr_.getLogicalWidth("Night Lights",
                                          cat->ptSize(FontStyle::SmallRegular));
  int wMU = 25 + fontMgr_.getLogicalWidth("Metric Units",
                                          cat->ptSize(FontStyle::SmallRegular));
  int wFS = 25 + fontMgr_.getLogicalWidth("Scale to Full Screen",
                                          cat->ptSize(FontStyle::SmallRegular));
  int toggleGap = 20;
  int wRow2 = wNL + toggleGap + wMU + toggleGap + wFS;
  int xRow2 = cx - wRow2 / 2;

  nightLightsRect_ = {xRow2, y, wNL, 20};
  drawToggle(nightLightsRect_, mapNightLights_, "Night Lights");
  metricToggleRect_ = {xRow2 + wNL + toggleGap, y, wMU, 20};
  drawToggle(metricToggleRect_, useMetric_, "Metric Units");
  scaleToFullScreenRect_ = {metricToggleRect_.x + wMU + toggleGap, y, wFS, 20};
  drawToggle(scaleToFullScreenRect_, scaleToFullScreen_, "Scale to Full Screen");
  y += 20 + vSpace;

  // --- Brightness section ---
  cat->drawText(renderer, "--- Brightness ---", cx, y, themes.accent,
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
                themes.text, FontStyle::SmallRegular, true, false, true);
  y += fieldH + vSpace;

  int wSchedule = 25 + fontMgr_.getLogicalWidth("Enable Dim/Bright Schedule",
                                              cat->ptSize(FontStyle::SmallRegular));
  scheduleToggleRect_ = {fieldX, y, wSchedule, 20};
  drawToggle(scheduleToggleRect_, brightnessMgr_.isScheduleEnabled(),
             "Enable Dim/Bright Schedule");
  y += 20 + vSpace;

  if (brightnessMgr_.isScheduleEnabled()) {
    cat->drawText(renderer, "Dim:", fieldX, y + 12, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    int halfFieldW = (fieldW - pad - 60) / 2;
    int halfW = fieldW / 2;
    int dimX = fieldX + 40;
    dimTimeRect_ = {dimX, y, halfFieldW, 24};
    dimTimeInput_.render(renderer, fontMgr_, dimX, y, halfFieldW, 24,
                         FontStyle::SmallRegular, textPad, activeField_ == 1,
                         true, themes.accent, themes.border, themes.text, themes.text, themes.textDim, "HH:MM", &themes.rowStripe1);

    cat->drawText(renderer, "Bright:", fieldX + halfW, y + 12, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    int brightX = fieldX + halfW + 55;
    brightTimeRect_ = {brightX, y, halfFieldW, 24};
    brightTimeInput_.render(renderer, fontMgr_, brightX, y, halfFieldW, 24,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            true, themes.accent, themes.border, themes.text, themes.text, themes.textDim, "HH:MM", &themes.rowStripe1);
    y += 24 + vSpace;
  } else {
    dimTimeRect_ = {0, 0, 0, 0};
    brightTimeRect_ = {0, 0, 0, 0};
  }

  // Row 5: Display Power Method
  {
    int wLabel = fontMgr_.getLogicalWidth("Display Power:", cat->ptSize(FontStyle::SmallRegular));
    cat->drawText(renderer, "Display Power:", fieldX, y + 12, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    powerMethodRect_ = {fieldX + wLabel + 10, y, 160, 24};
    drawBtn(powerMethodRect_, displayPowerMethod_);
    y += 24 + vSpace;
  }

#ifndef __EMSCRIPTEN__
  // --- Font section ---
  cat->drawText(renderer, "--- Font ---", cx, y, themes.accent,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  {
    int wLabel = fontMgr_.getLogicalWidth("Font:", cat->ptSize(FontStyle::SmallRegular));
    cat->drawText(renderer, "Font:", fieldX, y + fieldH / 2, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    std::string fontName = (fontListSelected_ == 0 || systemFonts_.empty())
                               ? "Built-in (Default)"
                               : systemFonts_[fontListSelected_ - 1].first;
    const int changeBtnW = 70;
    int nameW = fieldW - wLabel - 10 - 4 - changeBtnW;
    SDL_Rect nameRect = {fieldX + wLabel + 10, y, nameW, fieldH};
    fontListRect_ = {nameRect.x + nameW + 4, y, changeBtnW, fieldH};
    drawBtn(nameRect, fontName);
    drawBtn(fontListRect_, "Change...");
    y += fieldH + vSpace;
  }
#else
  fontListRect_ = {};
#endif
}

#ifndef __EMSCRIPTEN__
void SetupScreen::renderFontModal(SDL_Renderer *renderer) {
  if (!fontModalOpen_)
    return;

  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);
  int rowH = cat->ptSize(FontStyle::SmallRegular) + 10;
  int pad = 10;

  // Dim overlay
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
  SDL_RenderFillRect(renderer, &modalRect_);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Modal box
  int mw = std::min(500, modalRect_.w - 40);
  int mh = std::min(380, modalRect_.h - 60);
  int mx = modalRect_.x + (modalRect_.w - mw) / 2;
  int my = modalRect_.y + (modalRect_.h - mh) / 2;
  fontModalRect_ = {mx, my, mw, mh};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, themes.bg.a);
  SDL_RenderFillRect(renderer, &fontModalRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &fontModalRect_);

  int innerX = mx + pad;
  int innerW = mw - 2 * pad;
  int y = my + pad;

  // Title
  cat->drawText(renderer, "Select Font", mx + mw / 2, y + rowH / 2,
                themes.accent, FontStyle::SmallBold, true, false, true);
  y += rowH + pad / 2;

  // Filter input
  fontModalFilterRect_ = {innerX, y, innerW, rowH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &fontModalFilterRect_);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &fontModalFilterRect_);
  std::string filterDisplay = fontModalFilter_.empty() ? "Type to filter..." : fontModalFilter_ + "|";
  SDL_Color filterCol = fontModalFilter_.empty() ? themes.textDim : themes.text;
  cat->drawText(renderer, filterDisplay, innerX + 6, y + rowH / 2,
                filterCol, FontStyle::SmallRegular, false, false, true);
  y += rowH + pad / 2;

  // Build filtered list (-1 = built-in, >=0 = systemFonts_ index)
  fontModalFiltered_.clear();
  {
    std::string lf = fontModalFilter_;
    for (auto &c : lf)
      c = std::tolower(static_cast<unsigned char>(c));
    if (lf.empty() || std::string("built-in (default)").find(lf) != std::string::npos)
      fontModalFiltered_.push_back(-1);
    for (int i = 0; i < (int)systemFonts_.size(); ++i) {
      std::string lname = systemFonts_[i].first;
      for (auto &c : lname)
        c = std::tolower(static_cast<unsigned char>(c));
      if (lf.empty() || lname.find(lf) != std::string::npos)
        fontModalFiltered_.push_back(i);
    }
  }

  // List area (leave room for OK/Cancel row)
  int btnH = rowH;
  int listBottom = my + mh - pad - btnH - pad;
  int listH = listBottom - y;
  int visibleRows = std::max(1, listH / rowH);
  int maxScroll = std::max(0, (int)fontModalFiltered_.size() - visibleRows);
  fontModalScroll_ = std::max(0, std::min(fontModalScroll_, maxScroll));

  fontModalListRect_ = {innerX, y, innerW, visibleRows * rowH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &fontModalListRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &fontModalListRect_);

  for (int row = 0; row < visibleRows; ++row) {
    int idx = fontModalScroll_ + row;
    if (idx >= (int)fontModalFiltered_.size())
      break;
    int fontIdx = fontModalFiltered_[idx];
    int rowY = y + row * rowH;
    SDL_Rect rowRect = {innerX + 1, rowY, innerW - 2, rowH};
    bool isSelected = (fontIdx == -1) ? (fontModalSelected_ == 0)
                                      : (fontModalSelected_ == fontIdx + 1);
    if (isSelected) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 200);
      SDL_RenderFillRect(renderer, &rowRect);
    } else if (row % 2 == 1) {
      SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g, themes.rowStripe2.b, 255);
      SDL_RenderFillRect(renderer, &rowRect);
    }
    std::string name = (fontIdx == -1) ? "Built-in (Default)" : systemFonts_[fontIdx].first;
    SDL_Color tc = isSelected ? themes.bg : themes.text;
    cat->drawText(renderer, name, innerX + 8, rowY + rowH / 2, tc,
                  FontStyle::SmallRegular, false, false, true);
  }

  // Scrollbar
  if (maxScroll > 0) {
    int totalH = visibleRows * rowH;
    int barH = std::max(10, totalH * visibleRows / (int)fontModalFiltered_.size());
    int barY = (totalH - barH) * fontModalScroll_ / maxScroll;
    SDL_Rect scrollBar = {innerX + innerW - 5, y + barY, 4, barH};
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 180);
    SDL_RenderFillRect(renderer, &scrollBar);
  }

  y = listBottom + pad;

  // OK / Cancel buttons
  int okW = 80;
  int cancelW = 80;
  int totalBtnW = cancelW + 20 + okW;
  int btnX = mx + (mw - totalBtnW) / 2;
  fontModalCancelRect_ = {btnX, y, cancelW, btnH};
  fontModalOkRect_ = {btnX + cancelW + 20, y, okW, btnH};

  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &fontModalCancelRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &fontModalCancelRect_);
  cat->drawText(renderer, "Cancel", fontModalCancelRect_.x + cancelW / 2,
                fontModalCancelRect_.y + btnH / 2, themes.bg,
                FontStyle::SmallRegular, true, false, true);

  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
  SDL_RenderFillRect(renderer, &fontModalOkRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &fontModalOkRect_);
  cat->drawText(renderer, "OK", fontModalOkRect_.x + okW / 2,
                fontModalOkRect_.y + btnH / 2, themes.bg,
                FontStyle::SmallRegular, true, false, true);
}
#endif
