#include "PresetsModal.h"
#include "FontCatalog.h"
#include "../core/Theme.h"
#include "../core/WidgetType.h"

#include <algorithm>

PresetsModal::PresetsModal(FontManager &fontMgr) : fontMgr_(fontMgr) {}

void PresetsModal::init(AppConfig *cfg, std::function<void()> onApply) {
  cfg_ = cfg;
  onApply_ = std::move(onApply);
}

void PresetsModal::open() {
  if (!cfg_)
    return;
  active_ = true;
  saving_ = false;
  scrollOffset_ = 0;
  computeLayout();
}

void PresetsModal::computeLayout() {
  int mx = (kLogicalW - kModalW) / 2; // 190
  int my = (kLogicalH - kModalH) / 2; // 90
  dialogRect_ = {mx, my, kModalW, kModalH};

  // Title bar buttons (top-right)
  closeBtnRect_ = {mx + kModalW - 28, my + 4, 22, 22};
  scrollDnRect_ = {mx + kModalW - 54, my + 4, 22, 22};
  scrollUpRect_ = {mx + kModalW - 80, my + 4, 22, 22};

  // Save / name-input row
  int saveY = my + 34;
  saveAreaRect_ = {mx + 8, saveY, kModalW - 16, 28};
  saveBtnRect_ = {mx + 8, saveY, kModalW - 16, 28};
  // Name field takes most of the row; OK button on the right
  int okW = 42;
  nameFieldRect_ = {mx + 8, saveY, kModalW - 16 - okW - 6, 28};
  nameOkRect_ = {nameFieldRect_.x + nameFieldRect_.w + 6, saveY, okW, 28};

  // Built-in Contest row (sits between save area and user preset list)
  int contestRowY = my + 66;
  contestApplyRect_ = {mx + kModalW - 138, contestRowY + 3, 60, 24};

  // List area (pushed down by kRowH to make room for Contest row)
  int listY = my + 100;
  int listH = kVisibleRows * kRowH;
  listRect_ = {mx + 8, listY, kModalW - 16, listH};

  // Done button
  doneBtnRect_ = {mx + kModalW / 2 - 50, my + kModalH - 36, 100, 28};
}

// ── rendering helpers ────────────────────────────────────────────────────────

static bool ptInRect(int x, int y, const SDL_Rect &r) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void fillRect(SDL_Renderer *r, const SDL_Rect &rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

static void drawRect(SDL_Renderer *r, const SDL_Rect &rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderDrawRect(r, &rect);
}

void PresetsModal::render(SDL_Renderer *renderer) {
  if (!active_ || !cfg_)
    return;

  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(cfg_->theme);

  SDL_Color bgDark = themes.bg;
  SDL_Color border = themes.border;
  SDL_Color btnFill = themes.rowStripe1;
  SDL_Color btnBorder = themes.border;
  SDL_Color accent = themes.accent;
  SDL_Color white = themes.text;
  SDL_Color dimGray = themes.textDim;
  SDL_Color greenColor = themes.success;

  // ── Dialog background + border ───────────────────────────────────────────
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  fillRect(renderer, dialogRect_, bgDark);
  drawRect(renderer, dialogRect_, border);

  // ── Title bar ────────────────────────────────────────────────────────────
  SDL_Rect titleBg = {dialogRect_.x, dialogRect_.y, kModalW, 30};
  fillRect(renderer, titleBg, themes.rowStripe2);
  cat->drawText(renderer, "Presets", dialogRect_.x + 10, dialogRect_.y + 15,
                white, FontStyle::UIBold, false, false, true);

  // Scroll arrows (only useful if more than kVisibleRows presets)
  int total = cfg_ ? (int)cfg_->presets.size() : 0;
  bool canScrollUp = scrollOffset_ > 0;
  bool canScrollDn = scrollOffset_ + kVisibleRows < total;

  SDL_Color arrowCol = canScrollUp ? accent : dimGray;
  fillRect(renderer, scrollUpRect_, btnFill);
  drawRect(renderer, scrollUpRect_, btnBorder);
  cat->drawText(renderer, "^", scrollUpRect_.x + scrollUpRect_.w / 2,
                scrollUpRect_.y + scrollUpRect_.h / 2, arrowCol,
                FontStyle::UIBold, true, false, true);

  arrowCol = canScrollDn ? accent : dimGray;
  fillRect(renderer, scrollDnRect_, btnFill);
  drawRect(renderer, scrollDnRect_, btnBorder);
  cat->drawText(renderer, "v", scrollDnRect_.x + scrollDnRect_.w / 2,
                scrollDnRect_.y + scrollDnRect_.h / 2, arrowCol,
                FontStyle::UIBold, true, false, true);

  // Close [X] button
  fillRect(renderer, closeBtnRect_, themes.danger);
  drawRect(renderer, closeBtnRect_, themes.textDim);
  cat->drawText(renderer, "X", closeBtnRect_.x + closeBtnRect_.w / 2,
                closeBtnRect_.y + closeBtnRect_.h / 2, white, FontStyle::UIBold,
                true, false, true);

  // ── Save area ────────────────────────────────────────────────────────────
  if (!saving_) {
    // "★ Save Current Configuration" button
    fillRect(renderer, saveBtnRect_, btnFill);
    drawRect(renderer, saveBtnRect_, btnBorder);
    cat->drawText(renderer, "\xe2\x9c\xaf Save Current Configuration",
                  saveBtnRect_.x + saveBtnRect_.w / 2,
                  saveBtnRect_.y + saveBtnRect_.h / 2, accent, FontStyle::Fast,
                  true, false, true);
  } else {
    // Name input field
    SDL_Color fieldBg = themes.rowStripe1;
    SDL_Color fieldBorder = themes.accent;
    fillRect(renderer, nameFieldRect_, fieldBg);
    drawRect(renderer, nameFieldRect_, fieldBorder);

    const std::string &val = nameInput_.getValue();
    int tx = nameFieldRect_.x + 6;
    int ty = nameFieldRect_.y + 6;
    if (!val.empty()) {
      cat->drawText(renderer, val, tx, ty, white, FontStyle::Fast);
    } else {
      cat->drawText(renderer, "Name...", tx, ty, dimGray, FontStyle::Fast);
    }

    // Blinking cursor
    if ((SDL_GetTicks() / 500) % 2 == 0) {
      int cursorX = tx;
      if (!val.empty()) {
        int cp = nameInput_.getCursorPos();
        std::string before = val.substr(0, cp);
        cursorX +=
            fontMgr_.getLogicalWidth(before, cat->ptSize(FontStyle::Fast));
      }
      SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, themes.text.a);
      SDL_RenderDrawLine(renderer, cursorX, nameFieldRect_.y + 3, cursorX,
                         nameFieldRect_.y + nameFieldRect_.h - 3);
    }

    // OK button
    fillRect(renderer, nameOkRect_, themes.success);
    drawRect(renderer, nameOkRect_, themes.border);
    cat->drawText(renderer, "OK", nameOkRect_.x + nameOkRect_.w / 2,
                  nameOkRect_.y + nameOkRect_.h / 2, white, FontStyle::Fast,
                  true, false, true);
  }

  // ── Built-in Contest Mode row ────────────────────────────────────────────
  {
    int rowY = contestApplyRect_.y - 3;
    SDL_Rect rowBg = {listRect_.x, rowY, listRect_.w, kRowH};
    fillRect(renderer, rowBg, themes.rowStripe2);
    drawRect(renderer, rowBg, themes.border);
    cat->drawText(renderer, "\xe2\x98\x86 Contest Mode", listRect_.x + 8, rowY + 7,
                  accent, FontStyle::Fast);
    fillRect(renderer, contestApplyRect_, btnFill);
    drawRect(renderer, contestApplyRect_, btnBorder);
    cat->drawText(renderer, "Apply", contestApplyRect_.x + contestApplyRect_.w / 2,
                  contestApplyRect_.y + contestApplyRect_.h / 2, greenColor,
                  FontStyle::Caption, true, false, true);
  }

  // ── Separator ────────────────────────────────────────────────────────────
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, dialogRect_.x + 4, listRect_.y - 2,
                     dialogRect_.x + kModalW - 4, listRect_.y - 2);

  // ── Preset list ──────────────────────────────────────────────────────────
  // Clip to list area to prevent rows bleeding into title/footer
  SDL_RenderSetClipRect(renderer, &listRect_);

  rowRects_.clear();
  int visEnd = std::min(scrollOffset_ + kVisibleRows, total);
  for (int i = scrollOffset_; i < visEnd; ++i) {
    int rowY = listRect_.y + (i - scrollOffset_) * kRowH;
    const auto &p = cfg_->presets[i];

    // Alternating row background
    if ((i % 2) == 0) {
      SDL_Rect rowBg = {listRect_.x, rowY, listRect_.w, kRowH};
      fillRect(renderer, rowBg, themes.rowStripe2);
    }

    // Name text (clipped to left portion)
    SDL_Color nameCol = white;
    cat->drawText(renderer, p.name, listRect_.x + 8, rowY + 7, nameCol,
                  FontStyle::Fast);

    // [Apply] button
    SDL_Rect applyR = {listRect_.x + listRect_.w - 130, rowY + 3, 60, 24};
    fillRect(renderer, applyR, themes.rowStripe1);
    drawRect(renderer, applyR, themes.border);
    cat->drawText(renderer, "Apply", applyR.x + applyR.w / 2,
                  applyR.y + applyR.h / 2, greenColor, FontStyle::Caption, true,
                  false, true);

    // [✕] delete button
    SDL_Rect delR = {listRect_.x + listRect_.w - 64, rowY + 3, 24, 24};
    fillRect(renderer, delR, themes.danger);
    drawRect(renderer, delR, themes.border);
    cat->drawText(renderer, "X", delR.x + delR.w / 2, delR.y + delR.h / 2,
                  white, FontStyle::Caption, true, false, true);

    rowRects_.push_back({applyR, delR});
  }

  // Empty state message
  if (total == 0) {
    cat->drawText(renderer, "No presets saved yet.",
                  listRect_.x + listRect_.w / 2, listRect_.y + listRect_.h / 2,
                  dimGray, FontStyle::Fast, true, false, true);
  }

  SDL_RenderSetClipRect(renderer, nullptr);

  // ── Done button ──────────────────────────────────────────────────────────
  fillRect(renderer, doneBtnRect_, btnFill);
  drawRect(renderer, doneBtnRect_, btnBorder);
  cat->drawText(renderer, "Close", doneBtnRect_.x + doneBtnRect_.w / 2,
                doneBtnRect_.y + doneBtnRect_.h / 2, white, FontStyle::UIBold,
                true, false, true);
}

// ── input handling ───────────────────────────────────────────────────────────

bool PresetsModal::onMouseDown(int mx, int my, Uint16 /*mod*/) {
  if (!active_)
    return false;
  return ptInRect(mx, my, dialogRect_);
}

bool PresetsModal::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (!active_)
    return false;

  // Close button
  if (ptInRect(mx, my, closeBtnRect_) || !ptInRect(mx, my, dialogRect_)) {
    if (saving_)
      cancelSave();
    else
      active_ = false;
    return true;
  }

  // Scroll up
  if (ptInRect(mx, my, scrollUpRect_)) {
    if (scrollOffset_ > 0)
      scrollOffset_--;
    return true;
  }
  // Scroll down
  if (ptInRect(mx, my, scrollDnRect_)) {
    int total = cfg_ ? (int)cfg_->presets.size() : 0;
    if (scrollOffset_ + kVisibleRows < total)
      scrollOffset_++;
    return true;
  }

  // Save / name area
  if (!saving_) {
    if (ptInRect(mx, my, saveBtnRect_)) {
      beginSave();
      return true;
    }
  } else {
    if (ptInRect(mx, my, nameOkRect_)) {
      commitSave();
      return true;
    }
  }

  // Done button
  if (ptInRect(mx, my, doneBtnRect_)) {
    if (saving_)
      cancelSave();
    else
      active_ = false;
    return true;
  }

  // Contest Mode built-in apply
  if (ptInRect(mx, my, contestApplyRect_)) {
    cfg_->pane1Rotation = {WidgetType::DX_CLUSTER};
    cfg_->pane2Rotation = {WidgetType::LIVE_SPOTS};
    cfg_->pane3Rotation = {WidgetType::BAND_CONDITIONS};
    cfg_->pane4Rotation = {WidgetType::SOLAR};
    cfg_->pane5Rotation = {WidgetType::DE_INFO};
    cfg_->pane6Rotation = {WidgetType::DX_INFO};
    if (onApply_)
      onApply_();
    active_ = false;
    return true;
  }

  // Row buttons (apply / delete)
  for (int j = 0; j < (int)rowRects_.size(); ++j) {
    int idx = scrollOffset_ + j;
    if (ptInRect(mx, my, rowRects_[j].apply)) {
      applyPreset(idx);
      return true;
    }
    if (ptInRect(mx, my, rowRects_[j].del)) {
      deletePreset(idx);
      return true;
    }
  }

  return true; // swallow all clicks inside dialog
}

bool PresetsModal::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!active_)
    return false;

  if (saving_) {
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      commitSave();
      return true;
    case SDLK_ESCAPE:
      cancelSave();
      return true;
    default:
      nameInput_.onKeyDown(key, mod);
      return true;
    }
  }

  if (key == SDLK_ESCAPE) {
    active_ = false;
    return true;
  }
  return true;
}

bool PresetsModal::onTextInput(const char *text) {
  if (!active_ || !saving_)
    return false;
  nameInput_.onTextInput(text);
  return true;
}

// ── actions ──────────────────────────────────────────────────────────────────

void PresetsModal::beginSave() {
  saving_ = true;
  nameInput_.setValue("");
  nameInput_.setMaxLength(40);
  nameInput_.setCursorToEnd();
  nameInput_.setActive(true);
  SDL_StartTextInput();
}

void PresetsModal::cancelSave() {
  saving_ = false;
  SDL_StopTextInput();
}

void PresetsModal::commitSave() {
  if (!cfg_)
    return;
  std::string name = nameInput_.getValue();
  if (name.empty())
    name = "Preset " + std::to_string((int)cfg_->presets.size() + 1);

  ConfigPreset p;
  p.name = name;
  p.pane1Rotation = cfg_->pane1Rotation;
  p.pane2Rotation = cfg_->pane2Rotation;
  p.pane3Rotation = cfg_->pane3Rotation;
  p.pane4Rotation = cfg_->pane4Rotation;
  p.pane5Rotation = cfg_->pane5Rotation;
  p.pane6Rotation = cfg_->pane6Rotation;
  p.rotationIntervalS = cfg_->rotationIntervalS;
  p.propOverlay = cfg_->propOverlay;
  p.weatherOverlay = cfg_->weatherOverlay;
  p.mapStyle = cfg_->mapStyle;
  p.mapNightLights = cfg_->mapNightLights;
  p.showGrid = cfg_->showGrid;
  p.gridType = cfg_->gridType;
  p.propBand = cfg_->propBand;
  p.propMode = cfg_->propMode;
  p.propPower = cfg_->propPower;

  cfg_->presets.push_back(std::move(p));
  ConfigManager::instance().save(*cfg_);

  saving_ = false;
  SDL_StopTextInput();
}

void PresetsModal::applyPreset(int index) {
  if (!cfg_ || index < 0 || index >= (int)cfg_->presets.size())
    return;
  const auto &p = cfg_->presets[index];

  cfg_->pane1Rotation = p.pane1Rotation;
  cfg_->pane2Rotation = p.pane2Rotation;
  cfg_->pane3Rotation = p.pane3Rotation;
  cfg_->pane4Rotation = p.pane4Rotation;
  cfg_->pane5Rotation = p.pane5Rotation;
  cfg_->pane6Rotation = p.pane6Rotation;
  cfg_->rotationIntervalS = p.rotationIntervalS;
  cfg_->propOverlay = p.propOverlay;
  cfg_->weatherOverlay = p.weatherOverlay;
  cfg_->mapStyle = p.mapStyle;
  cfg_->mapNightLights = p.mapNightLights;
  cfg_->showGrid = p.showGrid;
  cfg_->gridType = p.gridType;
  cfg_->propBand = p.propBand;
  cfg_->propMode = p.propMode;
  cfg_->propPower = p.propPower;

  if (onApply_)
    onApply_();

  active_ = false;
}

void PresetsModal::deletePreset(int index) {
  if (!cfg_ || index < 0 || index >= (int)cfg_->presets.size())
    return;
  cfg_->presets.erase(cfg_->presets.begin() + index);
  ConfigManager::instance().save(*cfg_);
  // Clamp scroll
  int total = (int)cfg_->presets.size();
  if (scrollOffset_ > 0 && scrollOffset_ >= total - kVisibleRows + 1)
    scrollOffset_ = std::max(0, total - kVisibleRows);
}
