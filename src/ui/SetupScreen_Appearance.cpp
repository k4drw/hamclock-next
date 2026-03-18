#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>
#include <string>

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

  scheduleToggleRect_ = {fieldX, y, 20, 20};
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
                         true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "HH:MM", &themes.rowStripe1);

    cat->drawText(renderer, "Bright:", fieldX + halfW, y + 12, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    int brightX = fieldX + halfW + 55;
    brightTimeRect_ = {brightX, y, halfFieldW, 24};
    brightTimeInput_.render(renderer, fontMgr_, brightX, y, halfFieldW, 24,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "HH:MM", &themes.rowStripe1);
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
}
