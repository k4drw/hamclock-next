#include "RigControlPanel.h"
#include "../core/Theme.h"
#include "../services/RigService.h"
#include "FontCatalog.h"
#include <cstdio>

RigControlPanel::RigControlPanel(int x, int y, int w, int h, FontManager &fontMgr,
                                 RigService *rig)
    : Widget(x, y, w, h), fontMgr_(fontMgr), rig_(rig) {}

void RigControlPanel::update() {
  if (rig_)
    state_ = rig_->getState();
}

void RigControlPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors tc = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, tc.bg.r, tc.bg.g, tc.bg.b, tc.bg.a);
  SDL_Rect body = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &body);

  // Border
  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b, tc.border.a);
  SDL_RenderDrawRect(renderer, &body);

  auto *cat = fontMgr_.catalog();
  const int pad = 8;
  int cy = y_ + 4;

  // --- Title row ---
  cat->drawText(renderer, "Rig Control", x_ + pad, cy, tc.accent, FontStyle::MicroBold);

  // Connection indicator (dot)
  bool connected = state_.connected;
  SDL_Color dotColor = connected ? tc.success : tc.danger;
  SDL_Rect dot = {x_ + width_ - pad - 8, cy + 2, 8, 8};
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, dotColor.r, dotColor.g, dotColor.b, 255);
  SDL_RenderFillRect(renderer, &dot);

  cy += 20;

  if (!connected) {
    cat->drawText(renderer, "No rig", x_ + width_ / 2, y_ + height_ / 2,
                  tc.textDim, FontStyle::Fast, true);
    return;
  }

  // --- Frequency ---
  char freqBuf[24];
  long long f = state_.freqHz;
  if (f >= 1000000LL) {
    std::snprintf(freqBuf, sizeof(freqBuf), "%.4f MHz", f / 1e6);
  } else if (f >= 1000LL) {
    std::snprintf(freqBuf, sizeof(freqBuf), "%.1f kHz", f / 1e3);
  } else {
    std::snprintf(freqBuf, sizeof(freqBuf), "%lld Hz", f);
  }
  cat->drawText(renderer, freqBuf, x_ + width_ / 2, cy, tc.text,
                FontStyle::MediumBold, true);
  cy += 28;

  // --- Mode ---
  if (!state_.mode.empty()) {
    cat->drawText(renderer, state_.mode.c_str(), x_ + width_ / 2, cy,
                  tc.info, FontStyle::SmallBold, true);
  }
  cy += 22;

  // --- S-meter bar (placeholder: filled bar proportional to signal level) ---
  // RigData has no S-meter field; render a static "no S-meter" label for now.
  int barW = width_ - pad * 2;
  int barH = 8;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b, 255);
  SDL_Rect barBg = {x_ + pad, cy, barW, barH};
  SDL_RenderDrawRect(renderer, &barBg);
  // S-meter data not available in RigData; bar remains empty until CAT support added
  cat->drawText(renderer, "S-meter: N/A", x_ + pad, cy + barH + 2,
                tc.textDim, FontStyle::Tiny, false);
}

void RigControlPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}
