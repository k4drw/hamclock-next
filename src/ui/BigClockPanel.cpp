#include "../core/Constants.h"
#include "BigClockPanel.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"

#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#define localtime_r(t, tm) (localtime_s(tm, t) ? NULL : tm)
#define gmtime_r(t, tm) (gmtime_s(tm, t) ? NULL : tm)
#endif

// ─────────────────────────────────────────────────────────────────────────────

BigClockPanel::BigClockPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             AppConfig &config, ConfigManager &cfgMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), config_(config), cfgMgr_(cfgMgr) {
  onResize(x, y, w, h);
}

BigClockPanel::~BigClockPanel() {
  invalidateTextures();
}

void BigClockPanel::invalidateTextures() {
  if (hmTex_) {
    MemoryMonitor::getInstance().destroyTexture(hmTex_);
    hmTex_ = nullptr;
  }
  if (secTex_) {
    MemoryMonitor::getInstance().destroyTexture(secTex_);
    secTex_ = nullptr;
  }
  lastHM_.clear();
  lastSec_.clear();
}

void BigClockPanel::update() {
  // Time calculated live in render()
}

void BigClockPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Scale HH:MM to fill height; use smaller factor for dashboard pane than for full screen
  float factor = (h > (HamClock::LOGICAL_HEIGHT / 2)) ? 0.55f : 0.40f;
  hmPtSize_   = std::max(12, static_cast<int>(h * factor * 0.6f));
  secPtSize_  = hmPtSize_ * 6 / 10;
  datePtSize_ = std::max(10, h / 20);
  invalidateTextures();
}

// ─── Color helper ────────────────────────────────────────────────────────────

/*static*/ SDL_Color BigClockPanel::hueToColor(uint8_t hue) {
  // Hue 0-255 → 0-360°; S = 200/255 ≈ 0.784; V = 1.0
  // (matches original HSV_2_RGB565(hue, 200, 255))
  float h = hue / 255.0f * 360.0f;
  float s = 200.0f / 255.0f;
  float v = 1.0f;

  float c  = v * s;
  float x  = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m  = v - c;

  float r1, g1, b1;
  int sector = static_cast<int>(h / 60.0f) % 6;
  switch (sector) {
    case 0:  r1 = c; g1 = x; b1 = 0; break;
    case 1:  r1 = x; g1 = c; b1 = 0; break;
    case 2:  r1 = 0; g1 = c; b1 = x; break;
    case 3:  r1 = 0; g1 = x; b1 = c; break;
    case 4:  r1 = x; g1 = 0; b1 = c; break;
    default: r1 = c; g1 = 0; b1 = x; break;
  }
  return SDL_Color{
      static_cast<uint8_t>((r1 + m) * 255),
      static_cast<uint8_t>((g1 + m) * 255),
      static_cast<uint8_t>((b1 + m) * 255),
      255};
}

// ─── Digital rendering ───────────────────────────────────────────────────────

void BigClockPanel::renderDigital(SDL_Renderer *renderer) {
  auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm tmbuf;
  struct tm *tp = config_.bigClockUtc
                      ? gmtime_r(&now_c, &tmbuf)
                      : localtime_r(&now_c, &tmbuf);
  if (!tp) return;

  SDL_Color clockColor = hueToColor(config_.bigClockHue);
  ThemeColors themes   = getThemeColors(theme_);
  auto *cat            = fontMgr_.catalog();

  // Blink colon when seconds are hidden
  bool colonOn = config_.bigClockShowSec || ((SDL_GetTicks() / 500) % 2 == 0);
  char sep = colonOn ? ':' : ' ';

  char hmBuf[10];
  if (config_.bigClock12h) {
    int h12 = tp->tm_hour % 12;
    if (h12 == 0) h12 = 12;
    std::snprintf(hmBuf, sizeof(hmBuf), "%d%c%02d", h12, sep, tp->tm_min);
  } else {
    std::snprintf(hmBuf, sizeof(hmBuf), "%02d%c%02d", tp->tm_hour, sep, tp->tm_min);
  }

  char secBuf[4];
  std::snprintf(secBuf, sizeof(secBuf), "%02d", tp->tm_sec);

  char dateBuf[32];
  std::strftime(dateBuf, sizeof(dateBuf), "%a %d %b %Y", tp);

  // Rebuild HM texture only when string changes
  if (hmBuf != lastHM_) {
    if (hmTex_) MemoryMonitor::getInstance().destroyTexture(hmTex_);
    hmTex_ = fontMgr_.renderText(renderer, hmBuf, clockColor, hmPtSize_, &hmW_, &hmH_, true);
    lastHM_ = hmBuf;
  }

  if (config_.bigClockShowSec && secBuf != lastSec_) {
    if (secTex_) MemoryMonitor::getInstance().destroyTexture(secTex_);
    secTex_ = fontMgr_.renderText(renderer, secBuf, clockColor, secPtSize_, &secW_, &secH_, true);
    lastSec_ = secBuf;
  } else if (!config_.bigClockShowSec) {
    if (secTex_) {
      MemoryMonitor::getInstance().destroyTexture(secTex_);
      secTex_ = nullptr;
    }
    lastSec_.clear();
  }

  int cx = x_ + width_ / 2;
  int cy = y_ + height_ / 2;

  // Center digits exactly in the widget

  // Layout: HH:MM [SS] centered horizontally
  int totalW = hmW_ + (config_.bigClockShowSec && secTex_ ? secW_ + 4 : 0);
  int startX = cx - totalW / 2;

  if (hmTex_) {
    SDL_Rect dst = {startX, cy - hmH_ / 2, hmW_, hmH_};
    SDL_RenderCopy(renderer, hmTex_, nullptr, &dst);
  }
  if (config_.bigClockShowSec && secTex_) {
    // Align seconds baseline to HH:MM baseline (slight upward nudge)
    int secY = cy + hmH_ / 2 - secH_ - hmH_ / 10;
    SDL_Rect dst = {startX + hmW_ + 4, secY, secW_, secH_};
    SDL_RenderCopy(renderer, secTex_, nullptr, &dst);
  }

  if (config_.bigClockShowDate) {
    cat->drawText(renderer, dateBuf, cx, cy + hmH_ / 2 + 6,
                  themes.textDim, FontStyle::SmallBold, /*centered=*/true);
  }
}

// ─── Analog rendering ────────────────────────────────────────────────────────

void BigClockPanel::renderAnalog(SDL_Renderer *renderer) {
  std::time_t now_c = std::time(nullptr);
  struct tm tmbuf;
  struct tm *tp = config_.bigClockUtc
                      ? gmtime_r(&now_c, &tmbuf)
                      : localtime_r(&now_c, &tmbuf);
  if (!tp) return;

  SDL_Color clockColor = hueToColor(config_.bigClockHue);
  ThemeColors themes   = getThemeColors(theme_);
  auto *cat            = fontMgr_.catalog();

  float cx = x_ + width_  / 2.0f;
  float cy = y_ + height_ / 2.0f;

  // Center the clock face exactly; the optional date sits below the pivot

  float r     = std::min(width_, height_) / 2.0f * 0.86f;
  float bezR  = r * 1.04f;
  float hrR   = r * 0.55f;
  float mnR   = r * 0.80f;
  float scR   = r * 0.88f;

  // Discrete ticking movement (one-second steps)
  float totalSec = (float)tp->tm_sec;
  float totalMin = tp->tm_min + totalSec / 60.0f;
  float totalHr = (tp->tm_hour % 12) + totalMin / 60.0f;

  static constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
  float secAngle = kTwoPi * totalSec / 60.0f;
  float mnAngle  = kTwoPi * totalMin / 60.0f;
  float hrAngle  = kTwoPi * totalHr  / 12.0f;

  // Dim color for bezel
  SDL_Color bezColor = {static_cast<uint8_t>(clockColor.r / 3),
                        static_cast<uint8_t>(clockColor.g / 3),
                        static_cast<uint8_t>(clockColor.b / 3),
                        255};

  // Bezel & Face (Anti-Aliased)
  auto drawCircleAA = [&](float radius, SDL_Color color) {
    if (lineAATex_) {
      static constexpr int kSegments = 64;
      std::vector<SDL_FPoint> points;
      points.reserve(kSegments + 1);
      for (int i = 0; i <= kSegments; ++i) {
        float a = kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
        points.push_back({cx + radius * sinf(a), cy - radius * cosf(a)});
      }
      RenderUtils::drawPolylineTextured(renderer, lineAATex_, points.data(),
                                        points.size(), 1.5f, color, true);
    } else {
      RenderUtils::drawCircleOutline(renderer, cx, cy, radius, color);
    }
  };
  drawCircleAA(bezR, bezColor);
  drawCircleAA(r, clockColor);

  // Tick marks
  for (int i = 0; i < 60; ++i) {
    float a   = kTwoPi * i / 60.0f;
    float sa  = sinf(a);
    float ca  = cosf(a);
    bool isHour = (i % 5 == 0);

    float inner = isHour ? r * 0.84f : r * 0.91f;
    float outer = r * 0.97f;
    float thick = isHour ? r * 0.025f : 1.0f;

    RenderUtils::drawThickLine(renderer,
        cx + inner * sa, cy - inner * ca,
        cx + outer * sa, cy - outer * ca,
        thick, clockColor);
  }

  // Hour hand
  if (lineAATex_) {
    RenderUtils::drawThickLineTextured(renderer, lineAATex_, cx, cy,
                                       cx + hrR * sinf(hrAngle),
                                       cy - hrR * cosf(hrAngle), r * 0.040f,
                                       clockColor);
  } else {
    RenderUtils::drawThickLine(renderer, cx, cy, cx + hrR * sinf(hrAngle),
                               cy - hrR * cosf(hrAngle), r * 0.040f, clockColor);
  }

  // Minute hand
  if (lineAATex_) {
    RenderUtils::drawThickLineTextured(renderer, lineAATex_, cx, cy,
                                       cx + mnR * sinf(mnAngle),
                                       cy - mnR * cosf(mnAngle), r * 0.025f,
                                       clockColor);
  } else {
    RenderUtils::drawThickLine(renderer, cx, cy, cx + mnR * sinf(mnAngle),
                               cy - mnR * cosf(mnAngle), r * 0.025f, clockColor);
  }

  // Second hand (thin, contrasting)
  if (config_.bigClockShowSec) {
    SDL_Color secColor = themes.danger;
    if (lineAATex_) {
      RenderUtils::drawThickLineTextured(renderer, lineAATex_, cx, cy,
                                         cx + scR * sinf(secAngle),
                                         cy - scR * cosf(secAngle), 1.5f,
                                         secColor);
      // Counterweight tail
      RenderUtils::drawThickLineTextured(
          renderer, lineAATex_, cx, cy, cx - (r * 0.15f) * sinf(secAngle),
          cy + (r * 0.15f) * cosf(secAngle), 1.5f, secColor);
    } else {
      RenderUtils::drawThickLine(renderer, cx, cy, cx + scR * sinf(secAngle),
                                 cy - scR * cosf(secAngle), 1.5f, secColor);
      // Counterweight tail
      RenderUtils::drawThickLine(renderer, cx, cy,
                                 cx - (r * 0.15f) * sinf(secAngle),
                                 cy + (r * 0.15f) * cosf(secAngle), 1.5f,
                                 secColor);
    }
  }

  // Center dot
  RenderUtils::drawCircle(renderer, cx, cy, r * 0.030f, clockColor);

  // Date below face
  if (config_.bigClockShowDate) {
    char dateBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%a %d %b %Y", tp);
    cat->drawText(renderer, dateBuf,
                  static_cast<int>(cx), static_cast<int>(cy + r * 0.60f),
                  themes.textDim, FontStyle::SmallBold, /*centered=*/true);
  }
}

// ─── Main render ─────────────────────────────────────────────────────────────

void BigClockPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;
  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, "Big Clock");
  if (menuVisible_) return; // modal drawn separately via renderModal()
  if (config_.bigClockDigital)
    renderDigital(renderer);
  else
    renderAnalog(renderer);
}

// ─── Modal layout ────────────────────────────────────────────────────────────

void BigClockPanel::recalcModalLayout() {
  static constexpr int kModalW  = 300;
  static constexpr int kTitleH  = 28;
  static constexpr int kRowH    = 28;
  static constexpr int kSwatchD = 28; // swatch circle diameter
  static constexpr int kSwatchGap = 4;
  static constexpr int kBtnH    = 30;
  static constexpr int kPad     = 8;

  int swatchRowW  = kNumSwatches * kSwatchD + (kNumSwatches - 1) * kSwatchGap;
  int swatchRowH  = kSwatchD + 16; // circle + label space above
  int modalH = kPad + kTitleH + kNumOptions * kRowH + kPad
             + swatchRowH + kPad + kBtnH + kPad;

  int mx = x_ + (width_  - kModalW) / 2;
  int my = y_ + (height_ - modalH)  / 2;
  mx = std::max(x_, mx);
  my = std::max(y_, my);

  modalRect_ = {mx, my, kModalW, modalH};

  int ry = my + kPad + kTitleH;
  for (int i = 0; i < kNumOptions; ++i) {
    optRects_[i] = {mx + kPad, ry, kModalW - kPad * 2, kRowH};
    ry += kRowH;
  }

  ry += kPad; // gap before swatches
  int swatchStartX = mx + (kModalW - swatchRowW) / 2;
  for (int i = 0; i < kNumSwatches; ++i) {
    swatchRects_[i] = {swatchStartX + i * (kSwatchD + kSwatchGap),
                       ry + 16, kSwatchD, kSwatchD};
  }

  ry += swatchRowH + kPad;
  int btnW   = (kModalW - kPad * 3) / 2;
  doneRect_   = {mx + kPad,              ry, btnW, kBtnH};
  cancelRect_ = {mx + kPad * 2 + btnW,  ry, btnW, kBtnH};
}

// ─── Modal rendering ─────────────────────────────────────────────────────────

void BigClockPanel::renderModal(SDL_Renderer *renderer) {
  if (!menuVisible_) return;
  renderModalInternal(renderer);
}

void BigClockPanel::renderModalInternal(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 245);
  SDL_RenderFillRect(renderer, &modalRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &modalRect_);

  // Title
  int titleCx = modalRect_.x + modalRect_.w / 2;
  int titleCy = modalRect_.y + 8 + 14; // pad + half row
  cat->drawText(renderer, "Big Clock Settings", titleCx, titleCy,
                themes.accent, FontStyle::SmallBold, /*centered=*/true, false, /*vertCentered=*/true);

  // Option rows
  static const char *kLabels[kNumOptions] = {
      "Digital (off = Analog)",
      "12-hour format",
      "UTC time",
      "Show seconds",
      "Show date",
  };
  bool *tmpFlags[kNumOptions] = {
      &tmpDigital_, &tmp12h_, &tmpUtc_, &tmpShowSec_, &tmpShowDate_
  };

  for (int i = 0; i < kNumOptions; ++i) {
    bool on = *tmpFlags[i];
    SDL_Rect r = optRects_[i];

    if (on) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 50);
      SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 80);
    SDL_RenderDrawRect(renderer, &r);

    // Indicator square on left
    int ind = r.h - 10;
    SDL_Rect indR = {r.x + 4, r.y + 5, ind, ind};
    SDL_SetRenderDrawColor(renderer, on ? themes.accent.r : themes.bg.r,
                                     on ? themes.accent.g : themes.bg.g,
                                     on ? themes.accent.b : themes.bg.b, 255);
    SDL_RenderFillRect(renderer, &indR);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 200);
    SDL_RenderDrawRect(renderer, &indR);

    cat->drawText(renderer, kLabels[i],
                  r.x + ind + 10, r.y + r.h / 2,
                  on ? themes.text : themes.textDim,
                  FontStyle::SmallBold, false, false, /*vertCentered=*/true);
  }

  // Color label
  int swatchTop = swatchRects_[0].y - 14;
  cat->drawText(renderer, "Color:", modalRect_.x + modalRect_.w / 2, swatchTop,
                themes.textDim, FontStyle::MicroBold, /*centered=*/true, false, /*vertCentered=*/true);

  // Color swatches
  for (int i = 0; i < kNumSwatches; ++i) {
    SDL_Color sc = hueToColor(kSwatchHues[i]);
    SDL_Rect r = swatchRects_[i];
    float cx = r.x + r.w / 2.0f;
    float cy = r.y + r.h / 2.0f;
    RenderUtils::drawCircle(renderer, cx, cy, r.w / 2.0f, sc);

    // Selection ring for matching swatch (find closest hue)
    int diff = std::abs(static_cast<int>(tmpHue_) - static_cast<int>(kSwatchHues[i]));
    if (diff > 127) diff = 256 - diff;
    if (diff < 18) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_Rect outline = {r.x - 2, r.y - 2, r.w + 4, r.h + 4};
      SDL_RenderDrawRect(renderer, &outline);
    }
  }

  // Buttons
  auto drawBtn = [&](SDL_Rect br, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(renderer, &br);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 200);
    SDL_RenderDrawRect(renderer, &br);
    cat->drawText(renderer, label,
                  br.x + br.w / 2, br.y + br.h / 2,
                  themes.bg, FontStyle::SmallBold, /*centered=*/true, false, /*vertCentered=*/true);
  };

  drawBtn(doneRect_,   "Done",   themes.success);
  drawBtn(cancelRect_, "Cancel", themes.danger);
}

// ─── Input ───────────────────────────────────────────────────────────────────

bool BigClockPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!menuVisible_) {
    if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
      return false;

    // Ignore top 10% to allow PaneContainer title-click logic to work for widget selection
    if (my < y_ + height_ / 10)
      return false;

    // Copy live config to temp, open modal
    tmpDigital_  = config_.bigClockDigital;
    tmp12h_      = config_.bigClock12h;
    tmpUtc_      = config_.bigClockUtc;
    tmpShowSec_  = config_.bigClockShowSec;
    tmpShowDate_ = config_.bigClockShowDate;
    tmpHue_      = config_.bigClockHue;
    recalcModalLayout();
    menuVisible_ = true;
    return true;
  }

  // Eat all clicks outside modal rect
  if (!pointInRect(mx, my, modalRect_)) return true;

  if (pointInRect(mx, my, doneRect_)) {
    config_.bigClockDigital  = tmpDigital_;
    config_.bigClock12h      = tmp12h_;
    config_.bigClockUtc      = tmpUtc_;
    config_.bigClockShowSec  = tmpShowSec_;
    config_.bigClockShowDate = tmpShowDate_;
    config_.bigClockHue      = tmpHue_;
    cfgMgr_.save(config_);
    invalidateTextures();
    menuVisible_ = false;
    return true;
  }

  if (pointInRect(mx, my, cancelRect_)) {
    menuVisible_ = false;
    return true;
  }

  bool *tmpFlags[kNumOptions] = {
      &tmpDigital_, &tmp12h_, &tmpUtc_, &tmpShowSec_, &tmpShowDate_
  };
  for (int i = 0; i < kNumOptions; ++i) {
    if (pointInRect(mx, my, optRects_[i])) {
      *tmpFlags[i] = !(*tmpFlags[i]);
      return true;
    }
  }

  for (int i = 0; i < kNumSwatches; ++i) {
    if (pointInRect(mx, my, swatchRects_[i])) {
      tmpHue_ = kSwatchHues[i];
      return true;
    }
  }

  return true; // eat all modal clicks
}

// ─── Registration ────────────────────────────────────────────────────────────

#include "WidgetRegistry.h"
REGISTER_WIDGET("big_clock", "Big Clock", false, false, {
  return std::make_unique<BigClockPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.appCfg, deps.cfgMgr);
})
