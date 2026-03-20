#include "RigControlPanel.h"
#include "../core/Theme.h"
#include "../services/RigService.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

// Step-size and mode name tables match the button order in renderExpanded()
constexpr long long RigControlPanel::STEP_SIZES[6];

static const char *MODE_NAMES[8] = {
    "USB", "LSB", "CW", "CWR", "AM", "FM", "DIG", "PKT"};
static const char *STEP_LABELS[6] = {"1", "10", "100", "1k", "10k", "100k"};

RigControlPanel::RigControlPanel(int x, int y, int w, int h,
                                 FontManager &fontMgr, RigService *rig)
    : Widget(x, y, w, h), fontMgr_(fontMgr), rig_(rig) {}

void RigControlPanel::update() {
  if (rig_)
    state_ = rig_->getState();
}

void RigControlPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void RigControlPanel::clearButtonRects() {
  btnVfoA_ = btnVfoB_ = btnSwap_ = btnSplit_ = {0, 0, 0, 0};
  btnStepDn_ = btnStepUp_ = btnRitDn_ = btnRitUp_ = {0, 0, 0, 0};
  for (auto &r : btnModes_)
    r = {0, 0, 0, 0};
  for (auto &r : btnSteps_)
    r = {0, 0, 0, 0};
}

void RigControlPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors tc = getThemeColors(theme_);

  SDL_SetRenderDrawBlendMode(
      renderer,
      (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, tc.bg.r, tc.bg.g, tc.bg.b, tc.bg.a);
  SDL_Rect body = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &body);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b,
                         tc.border.a);
  SDL_RenderDrawRect(renderer, &body);

  if (width_ > 300)
    renderExpanded(renderer);
  else
    renderCompact(renderer);
}

// ---------------------------------------------------------------------------
// Compact read-only display (normal pane size)
// ---------------------------------------------------------------------------
void RigControlPanel::renderCompact(SDL_Renderer *renderer) {
  clearButtonRects();

  ThemeColors tc = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  const int pad = 8;
  int cy = y_ + 4;

  cat->drawText(renderer, "Rig Control", x_ + pad, cy, tc.accent,
                FontStyle::MicroBold);

  bool connected = state_.connected;
  SDL_Color dotColor = connected ? tc.success : tc.danger;
  SDL_Rect dot = {x_ + width_ - pad - 8, cy + 2, 8, 8};
  SDL_SetRenderDrawColor(renderer, dotColor.r, dotColor.g, dotColor.b, 255);
  SDL_RenderFillRect(renderer, &dot);

  cy += 20;

  if (!connected) {
    cat->drawText(renderer, "No rig", x_ + width_ / 2, y_ + height_ / 2,
                  tc.textDim, FontStyle::Fast, true);
    return;
  }

  char freqBuf[24];
  long long f = state_.freqHz;
  if (f >= 1000000LL)
    std::snprintf(freqBuf, sizeof(freqBuf), "%.4f MHz", f / 1e6);
  else if (f >= 1000LL)
    std::snprintf(freqBuf, sizeof(freqBuf), "%.1f kHz", f / 1e3);
  else
    std::snprintf(freqBuf, sizeof(freqBuf), "%lld Hz", f);

  cat->drawText(renderer, freqBuf, x_ + width_ / 2, cy, tc.text,
                FontStyle::MediumBold, true);
  cy += 28;

  if (!state_.mode.empty())
    cat->drawText(renderer, state_.mode.c_str(), x_ + width_ / 2, cy, tc.info,
                  FontStyle::SmallBold, true);
  cy += 22;

  // S-meter bar (proportional when level is available)
  int barW = width_ - pad * 2;
  int barH = 8;
  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b, 255);
  SDL_Rect barBg = {x_ + pad, cy, barW, barH};
  SDL_RenderDrawRect(renderer, &barBg);

  float level = state_.signalLevelDbm;
  if (level > -127.0f) {
    float normalized =
        std::max(0.0f, std::min(1.0f, (level + 127.0f) / 114.0f));
    int fill = (int)(normalized * (barW - 2));
    if (fill > 0) {
      SDL_Rect barFill = {x_ + pad + 1, cy + 1, fill, barH - 2};
      SDL_Color barColor =
          (normalized < 0.5f) ? tc.success
          : (normalized < 0.8f) ? tc.accent
                                 : tc.danger;
      SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, 255);
      SDL_RenderFillRect(renderer, &barFill);
    }
  } else {
    cat->drawText(renderer, "S: N/A", x_ + pad, cy + barH + 2, tc.textDim,
                  FontStyle::Tiny, false);
  }
}

// ---------------------------------------------------------------------------
// Expanded interactive layout (fills map area when maximized)
// ---------------------------------------------------------------------------
void RigControlPanel::renderExpanded(SDL_Renderer *renderer) {
  clearButtonRects();

  ThemeColors tc = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  const int pad = 10;
  int cx = x_ + width_ / 2;

  // Helper: draw a small labeled button, highlighted when active
  auto drawBtn = [&](SDL_Rect r, const char *label, bool active) {
    SDL_Color bg = active ? tc.accent : tc.rowStripe1;
    SDL_Color fg = active ? tc.bg : tc.text;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b,
                           255);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, fg,
                  FontStyle::Tiny, true);
  };

  // ---------------------------------------------------------------------------
  // Row 1: Connection dot + VFO label + large frequency + VFO A/B/SWAP buttons
  // ---------------------------------------------------------------------------
  int row1y = y_ + 8;

  bool connected = state_.connected;
  SDL_Color dotColor = connected ? tc.success : tc.danger;
  SDL_Rect dot = {x_ + pad, row1y + 8, 8, 8};
  SDL_SetRenderDrawColor(renderer, dotColor.r, dotColor.g, dotColor.b, 255);
  SDL_RenderFillRect(renderer, &dot);

  if (!connected) {
    cat->drawText(renderer, "No rig connected", cx, y_ + height_ / 2,
                  tc.textDim, FontStyle::MediumBold, true);
    return;
  }

  // VFO label (A or B)
  const char *vfoLabel = state_.vfoA ? "VFO-A" : "VFO-B";
  cat->drawText(renderer, vfoLabel, x_ + pad + 14, row1y + 2, tc.textDim,
                FontStyle::Tiny, false);

  // Large frequency (7 significant digits for e.g. 14.195.000)
  char freqBuf[24];
  long long f = state_.freqHz;
  if (f >= 1000000LL)
    std::snprintf(freqBuf, sizeof(freqBuf), "%.6f MHz", f / 1e6);
  else if (f >= 1000LL)
    std::snprintf(freqBuf, sizeof(freqBuf), "%.3f kHz", f / 1e3);
  else
    std::snprintf(freqBuf, sizeof(freqBuf), "%lld Hz", f);

  cat->drawText(renderer, freqBuf, cx, row1y, tc.text, FontStyle::MediumBold,
                true);

  // VFO-B frequency (dimmed, right-of-center)
  if (state_.freqHzB > 0) {
    char freqBbuf[24];
    long long fb = state_.freqHzB;
    if (fb >= 1000000LL)
      std::snprintf(freqBbuf, sizeof(freqBbuf), "B:%.4f", fb / 1e6);
    else
      std::snprintf(freqBbuf, sizeof(freqBbuf), "B:%lld", fb);
    cat->drawText(renderer, freqBbuf, cx + 160, row1y + 10, tc.textDim,
                  FontStyle::Tiny, false);
  }

  // VFO A / B / SWAP buttons (top-right)
  int btnH = 22, btnW = 28;
  int bx = x_ + width_ - pad - 3 * btnW - 8;
  btnVfoA_ = {bx, row1y, btnW, btnH};
  btnVfoB_ = {bx + btnW + 2, row1y, btnW, btnH};
  btnSwap_ = {bx + 2 * btnW + 4, row1y, btnW + 6, btnH};

  drawBtn(btnVfoA_, "A", state_.vfoA);
  drawBtn(btnVfoB_, "B", !state_.vfoA);
  drawBtn(btnSwap_, "SWAP", false);

  // ---------------------------------------------------------------------------
  // Row 2: Mode + passband | RIT value + ± | SPLIT button
  // ---------------------------------------------------------------------------
  int row2y = row1y + 40;

  char modeBuf[32];
  std::snprintf(modeBuf, sizeof(modeBuf), "%s  %d Hz", state_.mode.c_str(),
                state_.passbandHz);
  cat->drawText(renderer, modeBuf, x_ + pad, row2y, tc.info, FontStyle::SmallBold,
                false);

  // RIT
  char ritBuf[24];
  std::snprintf(ritBuf, sizeof(ritBuf), "RIT: %+lld Hz", state_.ritHz);
  cat->drawText(renderer, ritBuf, cx - 20, row2y, tc.textDim, FontStyle::Tiny,
                false);

  // RIT ± buttons
  btnRitDn_ = {cx + 80, row2y - 1, 20, 18};
  btnRitUp_ = {cx + 102, row2y - 1, 20, 18};
  drawBtn(btnRitDn_, "-", false);
  drawBtn(btnRitUp_, "+", false);

  // SPLIT button (right)
  btnSplit_ = {x_ + width_ - pad - 62, row2y - 1, 60, 18};
  drawBtn(btnSplit_, state_.split ? "SPLIT ON" : "SPLIT OFF", state_.split);

  // ---------------------------------------------------------------------------
  // Row 3: Mode selection buttons
  // ---------------------------------------------------------------------------
  int row3y = row2y + 24;
  int modeBtnW = (width_ - pad * 2 - 7 * 4) / 8;
  for (int i = 0; i < 8; ++i) {
    btnModes_[i] = {x_ + pad + i * (modeBtnW + 4), row3y, modeBtnW, 24};
    drawBtn(btnModes_[i], MODE_NAMES[i], state_.mode == MODE_NAMES[i]);
  }

  // ---------------------------------------------------------------------------
  // Row 4: Frequency step selector + − / + step buttons
  // ---------------------------------------------------------------------------
  int row4y = row3y + 30;
  cat->drawText(renderer, "Step:", x_ + pad, row4y + 6, tc.textDim,
                FontStyle::Tiny, false);

  int stepBtnW = 36;
  int stepStartX = x_ + pad + 40;
  for (int i = 0; i < 6; ++i) {
    btnSteps_[i] = {stepStartX + i * (stepBtnW + 3), row4y, stepBtnW, 22};
    drawBtn(btnSteps_[i], STEP_LABELS[i], stepIdx_ == i);
  }

  btnStepDn_ = {x_ + width_ - pad - 50, row4y, 24, 22};
  btnStepUp_ = {x_ + width_ - pad - 24, row4y, 24, 22};
  drawBtn(btnStepDn_, "-", false);
  drawBtn(btnStepUp_, "+", false);

  // ---------------------------------------------------------------------------
  // Row 5: S-meter bar with S-unit tick marks
  // ---------------------------------------------------------------------------
  int row5y = row4y + 30;
  int barW = width_ - pad * 2 - 70; // leave room for dBm label
  int barH = 14;

  float level = state_.signalLevelDbm;
  // Map -127 dBm (floor) .. -13 dBm (S9+60) → 0..1
  float normalized = std::max(0.0f, std::min(1.0f, (level + 127.0f) / 114.0f));
  int fill = (int)(normalized * (barW - 2));

  SDL_Rect barBg = {x_ + pad, row5y, barW, barH};
  SDL_SetRenderDrawColor(renderer, tc.rowStripe2.r, tc.rowStripe2.g,
                         tc.rowStripe2.b, 255);
  SDL_RenderFillRect(renderer, &barBg);

  if (fill > 0 && level > -127.0f) {
    SDL_Rect barFill = {x_ + pad + 1, row5y + 1, fill, barH - 2};
    SDL_Color barColor = (normalized < 0.5f)  ? tc.success
                         : (normalized < 0.8f) ? tc.accent
                                               : tc.danger;
    SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, 255);
    SDL_RenderFillRect(renderer, &barFill);
  }

  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b, 255);
  SDL_RenderDrawRect(renderer, &barBg);

  // S-unit tick marks: S1 S3 S5 S7 S9 +20 +40 +60
  static const struct {
    int dbm;
    const char *label;
  } sMarks[] = {{-121, "S1"}, {-109, "S3"}, {-97, "S5"},   {-85, "S7"},
                {-73, "S9"},  {-53, "+20"}, {-33, "+40"}, {-13, "+60"}};
  for (const auto &m : sMarks) {
    float frac = std::max(0.0f, std::min(1.0f, (m.dbm + 127.0f) / 114.0f));
    int tx = x_ + pad + (int)(frac * (barW - 2)) + 1;
    SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b,
                           255);
    SDL_RenderDrawLine(renderer, tx, row5y + barH, tx, row5y + barH + 3);
    cat->drawText(renderer, m.label, tx, row5y + barH + 5, tc.textDim,
                  FontStyle::Micro, true);
  }

  // dBm label to the right of bar
  char levelBuf[20];
  if (level > -127.0f)
    std::snprintf(levelBuf, sizeof(levelBuf), "%.0f dBm", level);
  else
    std::snprintf(levelBuf, sizeof(levelBuf), "S:N/A");
  cat->drawText(renderer, levelBuf, x_ + pad + barW + 6,
                row5y + barH / 2, tc.text, FontStyle::Tiny, false);

  // ---------------------------------------------------------------------------
  // Row 6: Spectrum area
  // ---------------------------------------------------------------------------
  int row6y = row5y + barH + 16;
  int specH = y_ + height_ - row6y - pad;
  if (specH < 30)
    return;

  SDL_Rect specRect = {x_ + pad, row6y, width_ - pad * 2, specH};
  SDL_SetRenderDrawColor(renderer, tc.rowStripe2.r, tc.rowStripe2.g,
                         tc.rowStripe2.b, 255);
  SDL_RenderFillRect(renderer, &specRect);
  SDL_SetRenderDrawColor(renderer, tc.border.r, tc.border.g, tc.border.b, 255);
  SDL_RenderDrawRect(renderer, &specRect);

  if (!state_.spectrum.empty()) {
    // Render spectrum: each sample is a vertical bar from the bottom
    int n = (int)state_.spectrum.size();
    int innerW = specRect.w - 2;
    int innerH = specRect.h - 2;
    SDL_SetRenderDrawColor(renderer, tc.accent.r, tc.accent.g, tc.accent.b,
                           255);
    for (int i = 0; i < n; ++i) {
      int sx = specRect.x + 1 + (int)((float)i / n * innerW);
      float amp = std::max(0.0f, std::min(1.0f, state_.spectrum[i]));
      int barH2 = (int)(amp * innerH);
      if (barH2 > 0) {
        SDL_RenderDrawLine(renderer, sx, specRect.y + specRect.h - 1 - barH2,
                           sx, specRect.y + specRect.h - 1);
      }
    }
  } else {
    // Placeholder text
    const char *specMsg =
        state_.spectrumSupported ? "Spectrum data unavailable"
                                 : "Spectrum: not supported by this rig";
    cat->drawText(renderer, specMsg, specRect.x + specRect.w / 2,
                  specRect.y + specRect.h / 2, tc.textDim, FontStyle::SmallBold,
                  true);
    // Sub-label explaining requirement
    if (!state_.spectrumSupported) {
      cat->drawText(renderer, "Requires compatible rig + Hamlib >= 4.x",
                    specRect.x + specRect.w / 2,
                    specRect.y + specRect.h / 2 + 18, tc.textDim,
                    FontStyle::Tiny, true);
    }
  }
}

// ---------------------------------------------------------------------------
// Mouse handling
// ---------------------------------------------------------------------------
bool RigControlPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  (void)mod;
  (void)clicks;

  if (width_ <= 300)
    return false; // compact mode: no interactive elements

  SDL_Rect r = {x_, y_, width_, height_};
  if (mx < r.x || mx >= r.x + r.w || my < r.y || my >= r.y + r.h)
    return false;

  if (!state_.connected || !rig_)
    return true; // consume but do nothing

  // VFO buttons
  if (hitTest(btnVfoA_, mx, my)) {
    rig_->setVFO(true);
    return true;
  }
  if (hitTest(btnVfoB_, mx, my)) {
    rig_->setVFO(false);
    return true;
  }
  if (hitTest(btnSwap_, mx, my)) {
    rig_->vfoSwap();
    return true;
  }
  if (hitTest(btnSplit_, mx, my)) {
    rig_->setSplit(!state_.split);
    return true;
  }

  // Mode buttons
  for (int i = 0; i < 8; ++i) {
    if (hitTest(btnModes_[i], mx, my)) {
      rig_->setMode(MODE_NAMES[i]);
      return true;
    }
  }

  // Step size selection
  for (int i = 0; i < 6; ++i) {
    if (hitTest(btnSteps_[i], mx, my)) {
      stepIdx_ = i;
      return true;
    }
  }

  // Frequency step ± buttons
  if (hitTest(btnStepDn_, mx, my)) {
    rig_->setFrequency(state_.freqHz - STEP_SIZES[stepIdx_]);
    return true;
  }
  if (hitTest(btnStepUp_, mx, my)) {
    rig_->setFrequency(state_.freqHz + STEP_SIZES[stepIdx_]);
    return true;
  }

  // RIT ± buttons
  if (hitTest(btnRitDn_, mx, my)) {
    rig_->setRIT(state_.ritHz - RIT_STEP_HZ);
    return true;
  }
  if (hitTest(btnRitUp_, mx, my)) {
    rig_->setRIT(state_.ritHz + RIT_STEP_HZ);
    return true;
  }

  return true; // consume all clicks in expanded area
}
