#include "ClockAuxPanel.h"

#include "../core/Constants.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// Sentinel value 999 means "use system local time (std::localtime)" —
// DST-aware.
static constexpr int kLocalSentinel = 999;

const ClockAuxPanel::TzPreset ClockAuxPanel::kPresets[] = {
    {kLocalSentinel, "Local"}, // system local time, DST-aware
    {0, "UTC"},
    {-5, "EST"},
    {-6, "CST"},
    {-7, "MST"},
    {-8, "PST"},
    {1, "CET"},
    {9, "JST"},
    {10, "AEST"},
};
const int ClockAuxPanel::kPresetCount = static_cast<int>(
    sizeof(ClockAuxPanel::kPresets) / sizeof(ClockAuxPanel::kPresets[0]));

ClockAuxPanel::ClockAuxPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             AppConfig &config, ConfigManager &cfgMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), config_(config), cfgMgr_(cfgMgr) {
  customOffsetInput_.setMaxLength(4);
  customLabelInput_.setMaxLength(8);
  syncFromConfig();
}

void ClockAuxPanel::syncFromConfig() {
  tzPresetIndex_ = -1; // assume custom
  for (int i = 0; i < kPresetCount; ++i) {
    if (kPresets[i].offset == config_.auxClockTzOffset &&
        config_.auxClockTzLabel == kPresets[i].label) {
      tzPresetIndex_ = i;
      break;
    }
  }
}

void ClockAuxPanel::update() {}

void ClockAuxPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);

  struct tm *gmt;
  if (config_.auxClockTzOffset == kLocalSentinel) {
    gmt = std::localtime(&now_c);
  } else {
    std::time_t tzTime = now_c + (config_.auxClockTzOffset * 3600);
    gmt = std::gmtime(&tzTime);
  }
  auto *cat = fontMgr_.catalog();

  int titleH = 20;
  std::string title = config_.auxClockTzLabel + " Time";
  cat->drawText(renderer, title, x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  char buf[64];
  std::strftime(buf, sizeof(buf), "%H:%M:%S", gmt);
  cat->drawText(renderer, buf, centerX, curY + timeFontSize_ / 2, themes.text,
                FontStyle::MediumBold, true);
  curY += timeFontSize_ + 12;

  std::strftime(buf, sizeof(buf), "%Y-%m-%d", gmt);
  cat->drawText(renderer, buf, centerX, curY, themes.text, FontStyle::Fast,
                true);
  curY += infoFontSize_ + 8;

  int doy = gmt->tm_yday + 1;
  double jd = (double)now_c / 86400.0 + 2440587.5;

  std::snprintf(buf, sizeof(buf), "DOY %03d", doy);
  cat->drawText(renderer, buf, centerX, curY, themes.textDim, FontStyle::Fast,
                true);
  curY += infoFontSize_ + 6;

  std::snprintf(buf, sizeof(buf), "JD %.2f", jd);
  cat->drawText(renderer, buf, centerX, curY, themes.textDim, FontStyle::Fast,
                true);
  curY += infoFontSize_ + 6;

  sdLineY_ = curY;
  switch (config_.auxClockStarMode) {
  case 0:
    std::snprintf(buf, sizeof(buf), "K %.1f",
                  (jd - 2548448.5) * (2635.10833 / 365.25));
    break;
  case 2: {
    double daysLeft = 2474649.5 - jd;
    if (daysLeft > 0) {
      int d = (int)daysLeft;
      int h = (int)((daysLeft - d) * 24.0);
      std::snprintf(buf, sizeof(buf), "C %dd %dh", d, h);
    } else {
      std::snprintf(buf, sizeof(buf), "C Warp 1!");
    }
    break;
  }
  default:
    std::snprintf(buf, sizeof(buf), "P %.1f", (jd - 2567877.0) / 0.397766856);
    break;
  }
  cat->drawText(renderer, buf, centerX, curY, themes.textDim, FontStyle::Fast,
                true);

  if (height_ > 100) {
    cat->drawText(renderer, "tap to set TZ", centerX, y_ + height_ - 10,
                  themes.textDim, FontStyle::Tiny, true);
  }
}

void ClockAuxPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  timeFontSize_ = cat->ptSize(FontStyle::SmallBold);
  infoFontSize_ = cat->ptSize(FontStyle::Fast);

  if (h > 120) {
    timeFontSize_ = cat->ptSize(FontStyle::SmallBold) * 1.5;
  }
}

// ── Popup layout ─────────────────────────────────────────────────────────────

void ClockAuxPanel::recalcMenuLayout() {
  constexpr int mW = 264;
  constexpr int rowH = 26;
  constexpr int localRowH = rowH + 10; // extra room for DST sub-label
  constexpr int titleH = 28;
  constexpr int fieldH = 22;
  constexpr int customSectionH =
      8 + fieldH + 6 + fieldH +
      6; // top-pad + offset + gap + label + bottom-pad
  constexpr int btnH = 30;
  constexpr int pad = 8;
  constexpr int mH = titleH + localRowH + (kTotalRows - 1) * rowH + pad +
                     customSectionH + pad + btnH + pad;

  int mx = (HamClock::LOGICAL_WIDTH - mW) / 2;
  int my = (HamClock::LOGICAL_HEIGHT - mH) / 2;
  menuRect_ = {mx, my, mW, mH};

  int ry = my + titleH;
  for (int i = 0; i < kTotalRows; ++i) {
    int h = (i == 0) ? localRowH : rowH;
    rowRects_[i] = {mx, ry, mW, h};
    ry += h;
  }

  // Custom text fields
  constexpr int labelColW = 52;
  int fieldX = mx + labelColW + 4;
  int fieldW = mW - labelColW - 4 - pad;
  ry += pad;
  customOffsetRect_ = {fieldX, ry + 2, fieldW, fieldH};
  customLabelRect_ = {fieldX, ry + fieldH + 8, fieldW, fieldH};

  ry += customSectionH + pad;

  // Buttons
  constexpr int btnW = 100;
  cancelRect_ = {mx + pad, ry, btnW, btnH};
  doneRect_ = {mx + mW - pad - btnW, ry, btnW, btnH};
}

// ── Modal rendering
// ───────────────────────────────────────────────────────────

void ClockAuxPanel::renderModal(SDL_Renderer *renderer) {
  if (!menuVisible_)
    return;
  renderMenu(renderer);
}

void ClockAuxPanel::renderMenu(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Popup background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &menuRect_);

  // Title — match SDOPanel: themes.textDim, SmallRegular, centered
  cat->drawText(renderer, "Select Timezone", menuRect_.x + menuRect_.w / 2,
                menuRect_.y + 5, themes.textDim, FontStyle::SmallRegular, true);

  // Rows: 8 presets + "Custom..."
  for (int i = 0; i < kTotalRows; ++i) {
    const SDL_Rect &r = rowRects_[i];
    bool selected = (i < kPresetCount)
                        ? (!customSelected_ && tempPresetIdx_ == i)
                        : customSelected_;

    if (selected) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 80);
      SDL_RenderFillRect(renderer, &r);
    }

    // Row label — match SDOPanel: themes.text/textDim, FontStyle::UI
    char rowLabel[48];
    bool isLocal = (i < kPresetCount && kPresets[i].offset == kLocalSentinel);
    if (i < kPresetCount) {
      int off = kPresets[i].offset;
      if (isLocal)
        std::snprintf(rowLabel, sizeof(rowLabel), "Local");
      else if (off >= 0)
        std::snprintf(rowLabel, sizeof(rowLabel), "%s  (UTC+%d)",
                      kPresets[i].label, off);
      else
        std::snprintf(rowLabel, sizeof(rowLabel), "%s  (UTC%d)",
                      kPresets[i].label, off);
    } else {
      std::snprintf(rowLabel, sizeof(rowLabel), "Custom...");
    }

    SDL_Color rowColor = selected ? themes.text : themes.textDim;
    cat->drawText(renderer, rowLabel, r.x + 10, r.y + 4, rowColor,
                  FontStyle::UI);

    // Sub-label for Local: explain DST tracking
    if (isLocal) {
      cat->drawText(renderer, "follows DST automatically", r.x + 10, r.y + 15,
                    themes.textDim, FontStyle::Micro);
    }
  }

  // Custom input fields
  SDL_Color labelCol = customSelected_ ? themes.text : themes.textDim;
  int baseY = customOffsetRect_.y;
  cat->drawText(renderer, "UTC+/-", menuRect_.x + 4, baseY + 3, labelCol,
                FontStyle::UI);
  cat->drawText(renderer, "Label", menuRect_.x + 4,
                baseY + customOffsetRect_.h + 9, labelCol, FontStyle::UI);

  bool offAct = customSelected_ && offsetActive_;
  bool lblAct = customSelected_ && !offsetActive_;

  customOffsetInput_.render(renderer, fontMgr_, customOffsetRect_.x,
                            customOffsetRect_.y, customOffsetRect_.w,
                            customOffsetRect_.h, FontStyle::UI, 4, offAct, true,
                            themes.accent, themes.border, themes.text,
                            themes.text, themes.textDim, "-5");

  customLabelInput_.render(renderer, fontMgr_, customLabelRect_.x,
                           customLabelRect_.y, customLabelRect_.w,
                           customLabelRect_.h, FontStyle::UI, 4, lblAct, true,
                           themes.accent, themes.border, themes.text,
                           themes.text, themes.textDim, "EST");

  // Buttons — match SDOPanel lambda style
  auto drawBtn = [&](const SDL_Rect &r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 150);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, themes.text,
                  FontStyle::SmallBold, true, false, true);
  };

  drawBtn(doneRect_, "Done", themes.success);
  drawBtn(cancelRect_, "Cancel", themes.danger);
}

// ── Event handling
// ────────────────────────────────────────────────────────────

bool ClockAuxPanel::onMouseUp(int mx, int my, Uint16 /*mod*/, int /*clicks*/) {
  if (!menuVisible_) {
    if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
      return false;

    // Top 10% -> widget selection (let PaneContainer handle it)
    if (my < y_ + height_ / 10)
      return false;

    if (sdLineY_ > 0 && my >= sdLineY_ - 2 &&
        my <= sdLineY_ + infoFontSize_ + 4) {
      config_.auxClockStarMode = (config_.auxClockStarMode + 1) % 3;
      cfgMgr_.save(config_);
      return true;
    }

    // Open menu; sync temp state from current config
    tempPresetIdx_ = (tzPresetIndex_ >= 0) ? tzPresetIndex_ : 0;
    customSelected_ = (tzPresetIndex_ < 0);
    if (customSelected_) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%d", config_.auxClockTzOffset);
      customOffsetInput_.setValue(buf);
      customLabelInput_.setValue(config_.auxClockTzLabel);
    } else {
      customOffsetInput_.clear();
      customLabelInput_.clear();
    }
    customOffsetInput_.setActive(false);
    customLabelInput_.setActive(false);
    offsetActive_ = true;
    recalcMenuLayout();
    menuVisible_ = true;
    return true;
  }

  // Eat all clicks outside the popup while menu is open
  if (mx < menuRect_.x || mx >= menuRect_.x + menuRect_.w || my < menuRect_.y ||
      my >= menuRect_.y + menuRect_.h)
    return true;

  // Done
  if (mx >= doneRect_.x && mx < doneRect_.x + doneRect_.w &&
      my >= doneRect_.y && my < doneRect_.y + doneRect_.h) {
    if (customSelected_) {
      std::string offStr = customOffsetInput_.getValue();
      int off = offStr.empty() ? 0 : std::atoi(offStr.c_str());
      off = std::clamp(off, -12, 14);
      std::string lbl = customLabelInput_.getValue();
      if (lbl.empty())
        lbl = (off >= 0 ? "UTC+" : "UTC") + std::to_string(off);
      config_.auxClockTzOffset = off;
      config_.auxClockTzLabel = lbl;
    } else {
      config_.auxClockTzOffset = kPresets[tempPresetIdx_].offset;
      config_.auxClockTzLabel = kPresets[tempPresetIdx_].label;
    }
    tzPresetIndex_ = customSelected_ ? -1 : tempPresetIdx_;
    cfgMgr_.save(config_);
    menuVisible_ = false;
    return true;
  }

  // Cancel
  if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
      my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
    menuVisible_ = false;
    return true;
  }

  // Preset/Custom rows
  for (int i = 0; i < kTotalRows; ++i) {
    const SDL_Rect &r = rowRects_[i];
    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
      if (i < kPresetCount) {
        tempPresetIdx_ = i;
        customSelected_ = false;
        customOffsetInput_.setActive(false);
        customLabelInput_.setActive(false);
      } else {
        customSelected_ = true;
        if (customOffsetInput_.getValue().empty()) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "%d", config_.auxClockTzOffset);
          customOffsetInput_.setValue(buf);
          customLabelInput_.setValue(config_.auxClockTzLabel);
        }
        offsetActive_ = true;
        customOffsetInput_.setActive(true);
        customLabelInput_.setActive(false);
        customOffsetInput_.setCursorToEnd();
      }
      return true;
    }
  }

  // Click directly in offset field
  if (customSelected_ && mx >= customOffsetRect_.x &&
      mx < customOffsetRect_.x + customOffsetRect_.w &&
      my >= customOffsetRect_.y &&
      my < customOffsetRect_.y + customOffsetRect_.h) {
    offsetActive_ = true;
    customOffsetInput_.setActive(true);
    customLabelInput_.setActive(false);
    return true;
  }

  // Click directly in label field
  if (customSelected_ && mx >= customLabelRect_.x &&
      mx < customLabelRect_.x + customLabelRect_.w &&
      my >= customLabelRect_.y &&
      my < customLabelRect_.y + customLabelRect_.h) {
    offsetActive_ = false;
    customOffsetInput_.setActive(false);
    customLabelInput_.setActive(true);
    return true;
  }

  return true; // eat all clicks while menu is open
}

bool ClockAuxPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!menuVisible_)
    return false;

  if (key == SDLK_ESCAPE) {
    menuVisible_ = false;
    return true;
  }

  if (!customSelected_)
    return true; // eat keys but no text routing needed

  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    if (offsetActive_) {
      // Tab to label field
      offsetActive_ = false;
      customOffsetInput_.setActive(false);
      customLabelInput_.setActive(true);
      customLabelInput_.setCursorToEnd();
    } else {
      // Commit
      std::string offStr = customOffsetInput_.getValue();
      int off = offStr.empty() ? 0 : std::atoi(offStr.c_str());
      off = std::clamp(off, -12, 14);
      std::string lbl = customLabelInput_.getValue();
      if (lbl.empty())
        lbl = (off >= 0 ? "UTC+" : "UTC") + std::to_string(off);
      config_.auxClockTzOffset = off;
      config_.auxClockTzLabel = lbl;
      tzPresetIndex_ = -1;
      cfgMgr_.save(config_);
      menuVisible_ = false;
    }
    return true;
  }

  if (key == SDLK_TAB) {
    offsetActive_ = !offsetActive_;
    customOffsetInput_.setActive(offsetActive_);
    customLabelInput_.setActive(!offsetActive_);
    if (offsetActive_)
      customOffsetInput_.setCursorToEnd();
    else
      customLabelInput_.setCursorToEnd();
    return true;
  }

  if (offsetActive_)
    return customOffsetInput_.onKeyDown(key, mod);
  return customLabelInput_.onKeyDown(key, mod);
}

bool ClockAuxPanel::onTextInput(const char *text) {
  if (!menuVisible_ || !customSelected_)
    return false;
  if (offsetActive_)
    return customOffsetInput_.onTextInput(text);
  return customLabelInput_.onTextInput(text);
}
