#include "TimePanel.h"
#include "../core/Astronomy.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include "../core/TimeUtils.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include "TextureManager.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/statvfs.h>
#endif

namespace {

static constexpr Uint32 kInfoRotateMs = 3000;

std::string getSystemUptime() {
#ifdef _WIN32
  uint64_t ms = GetTickCount64();
  double sec = ms / 1000.0;
#else
  std::FILE *f = std::fopen("/proc/uptime", "r");
  if (!f)
    return "Up ?";
  double sec = 0;
  if (std::fscanf(f, "%lf", &sec) != 1)
    sec = 0;
  std::fclose(f);
#endif
  int days = static_cast<int>(sec / 86400);
  int hours = static_cast<int>(sec / 3600) % 24;
  int mins = static_cast<int>(sec / 60) % 60;
  char buf[32];
  if (days > 0)
    std::snprintf(buf, sizeof(buf), "Up  %dd %dh", days, hours);
  else if (hours > 0)
    std::snprintf(buf, sizeof(buf), "Up  %dh %dm", hours, mins);
  else
    std::snprintf(buf, sizeof(buf), "Up  %dm", mins);
  return buf;
}

std::string getCpuTemp(bool metric) {
#ifdef _WIN32
  return "CPU --";
#else
  std::FILE *f = std::fopen("/sys/class/thermal/thermal_zone0/temp", "r");
  if (!f)
    return "CPU --";
  int milliC = 0;
  if (std::fscanf(f, "%d", &milliC) != 1)
    milliC = 0;
  std::fclose(f);
  char buf[32];
  if (metric) {
    std::snprintf(buf, sizeof(buf), "CPU %.0fC", milliC / 1000.0);
  } else {
    double tempF = milliC / 1000.0 * 9.0 / 5.0 + 32.0;
    std::snprintf(buf, sizeof(buf), "CPU %.0fF", tempF);
  }
  return buf;
#endif
}

std::string getDiskUsage() {
  int pct = MemoryMonitor::getInstance().getDiskUsagePct();
  if (pct < 0)
    return "Disk --";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "Disk %d%%", pct);
  return buf;
}

std::string getLocalIP() {
#ifdef _WIN32
  return "L-IP --";
#else
  struct ifaddrs *addrs = nullptr;
  if (getifaddrs(&addrs) != 0)
    return "--";
  std::string result = "L-IP --";
  for (struct ifaddrs *ifa = addrs; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;
    if (std::strcmp(ifa->ifa_name, "lo") == 0)
      continue;
    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
    result = std::string("") + ip;
    break;
  }
  freeifaddrs(addrs);
  return result;
#endif
}

} // namespace

TimePanel::TimePanel(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr, const std::string &callsign)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      callsign_(callsign) {}

void TimePanel::destroyCache() {
  MemoryMonitor::getInstance().destroyTexture(callTex_);
  MemoryMonitor::getInstance().destroyTexture(hmTex_);
  MemoryMonitor::getInstance().destroyTexture(secTex_);
  MemoryMonitor::getInstance().destroyTexture(dateTex_);
  MemoryMonitor::getInstance().destroyTexture(starTex_);
}

void TimePanel::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  char buf[32];
  currentHM_ = TimeUtils::hm(utc.tm_hour, utc.tm_min);

  std::snprintf(buf, sizeof(buf), "%02d", utc.tm_sec);
  currentSec_ = buf;

  static constexpr const char *kDays[] = {"Sun", "Mon", "Tue", "Wed",
                                          "Thu", "Fri", "Sat"};
  static constexpr const char *kMonths[] = {"Jan", "Feb", "Mar", "Apr",
                                            "May", "Jun", "Jul", "Aug",
                                            "Sep", "Oct", "Nov", "Dec"};
  std::snprintf(buf, sizeof(buf), "%s, %d %s %04d", kDays[utc.tm_wday],
                utc.tm_mday, kMonths[utc.tm_mon], 1900 + utc.tm_year);
  currentDate_ = buf;

  // System info: uptime every second, rotating values every 3 seconds
  currentUptime_ = getSystemUptime();

  Uint32 ticks = SDL_GetTicks();
  if (ticks - lastInfoRotateMs_ >= kInfoRotateMs) {
    infoRotateIdx_ = (infoRotateIdx_ + 1) % 3;
    lastInfoRotateMs_ = ticks;
    infoTexts_[0] = getCpuTemp(useMetric_);
    infoTexts_[1] = getDiskUsage();
    infoTexts_[2] = getLocalIP();
  }
  // Seed on first call
  if (infoTexts_[0].empty()) {
    infoTexts_[0] = getCpuTemp(useMetric_);
    infoTexts_[1] = getDiskUsage();
    infoTexts_[2] = getLocalIP();
  }
}

void TimePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  int pad = std::max(4, static_cast<int>(width_ * 0.03f));

  // 4-row layout proportional to 148px:
  // Callsign: 50 (was 42), Info bar: 16, Clock: 50 (was 58), Date: 32
  int callRowH = height_ * 50 / 148;
  int infoRowH = height_ * 16 / 148;
  int timeRowH = height_ * 50 / 148;

  int callBaseY = y_;
  int infoBaseY = callBaseY + callRowH;
  int timeBaseY = infoBaseY + infoRowH;
  int dateBaseY = timeBaseY + timeRowH;
  int dateRowH = y_ + height_ - dateBaseY;

  // --- Callsign (large, user-selected color, centered) ---
  if (callBgColor_.a > 0) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect bgRect = {x_ + 1, callBaseY + 1, width_ - 2, callRowH - 2};
    SDL_SetRenderDrawColor(renderer, callBgColor_.r, callBgColor_.g,
                           callBgColor_.b, callBgColor_.a);
    SDL_RenderFillRect(renderer, &bgRect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  }
  if (callFontSize_ != lastCallFontSize_ || !callTex_) {
    MemoryMonitor::getInstance().destroyTexture(callTex_);
    callTex_ = fontMgr_.renderText(renderer, callsign_, callColor_,
                                   callFontSize_, &callW_, &callH_, true);
    lastCallFontSize_ = callFontSize_;
  }
  if (callTex_) {
    int dy = callBaseY + (callRowH - callH_) / 2;
    int dx = x_ + (width_ - callW_) / 2;
    SDL_Rect dst = {dx, dy, callW_, callH_};
    SDL_RenderCopy(renderer, callTex_, nullptr, &dst);
  }

  // --- Gear icon + rotation transport controls (bottom-right corner) ---
  if (!editing_) {
    float gcx = gearRect_.x + gearRect_.w / 2.0f;
    float gcy = gearRect_.y + gearRect_.h / 2.0f;
    float r = gearSize_ / 2.0f;
    SDL_Color gearColor = themes.textDim;
    SDL_Color bgColor = themes.bg;
    RenderUtils::drawGear(renderer, gcx, gcy, r, gearColor, bgColor);

    // Pause / Next buttons — stacked above the gear icon (same column)
    auto *cat = fontMgr_.catalog();
    if (cat) {
      int btnSize = gearSize_ + 4;
      int btnGap = 3;
      int btnX = gearRect_.x + (gearRect_.w - btnSize) / 2;

      // Pause/Resume — directly above gear (persistent state, close to gear)
      pauseRect_ = {btnX, gearRect_.y - btnSize - btnGap, btnSize, btnSize};
      SDL_Color pauseBg = rotationPaused_ ? themes.accent : themes.rowStripe1;
      SDL_SetRenderDrawColor(renderer, pauseBg.r, pauseBg.g, pauseBg.b, 255);
      SDL_RenderFillRect(renderer, &pauseRect_);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &pauseRect_);

      const char *pauseLabel = rotationPaused_ ? "|>" : "||";
      SDL_Color pauseColor = rotationPaused_ ? themes.bg : themes.text;
      cat->drawText(renderer, pauseLabel,
                    pauseRect_.x + pauseRect_.w / 2,
                    pauseRect_.y + pauseRect_.h / 2,
                    pauseColor, FontStyle::Caption, true, false, true);

      // Next — above Pause (one-shot action floats to top)
      nextRect_ = {btnX, pauseRect_.y - btnSize - btnGap, btnSize, btnSize};
      SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
      SDL_RenderFillRect(renderer, &nextRect_);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &nextRect_);

      cat->drawText(renderer, ">>",
                    nextRect_.x + nextRect_.w / 2,
                    nextRect_.y + nextRect_.h / 2,
                    themes.text, FontStyle::Caption, true, false, true);
    }
  }

  // --- Info bar: uptime (left), rotating center, version (right) ---
  {
    SDL_Color gray = themes.textDim;
    int infoY = infoBaseY + (infoRowH - infoFontSize_) / 2;
    auto *cat = fontMgr_.catalog();
    TTF_Font *infoFont = fontMgr_.getFont(infoFontSize_);
    if (infoFont) {
      int tw = 0, th = 0;

      // Left: uptime
      cat->drawText(renderer, currentUptime_, x_ + pad, infoY, gray,
                    FontStyle::Fast);

      // Rotating info (shifted slightly right to give more room for uptime)
      const std::string &centerText = infoTexts_[infoRotateIdx_];
      TTF_SizeUTF8(infoFont, centerText.c_str(), &tw, &th);
      cat->drawText(renderer, centerText, x_ + (width_ - tw) * 0.58f, infoY,
                    gray, FontStyle::Fast);

      // Right: version (amber + asterisk when update available)
      std::string verStr = HAMCLOCK_VERSION;
      SDL_Color verColor = gray;
      if (updateAvailable_) {
        verStr += "*";
        verColor = themes.warning; // amber
      }
      TTF_SizeUTF8(infoFont, verStr.c_str(), &tw, &th);
      int vx = x_ + width_ - pad - tw;
      cat->drawText(renderer, verStr, vx, infoY, verColor, FontStyle::Fast);
      versionRect_ = {vx, infoY, tw, th};
    }
  }

  // --- Time: HH:MM (large, white) + SS (superscript, gray) ---
  if (!hmTex_ || currentHM_ != lastHM_ || hmFontSize_ != lastHmFontSize_) {
    MemoryMonitor::getInstance().destroyTexture(hmTex_);
    hmTex_ = fontMgr_.renderText(renderer, currentHM_, themes.text, hmFontSize_,
                                 &hmW_, &hmH_);
    lastHM_ = currentHM_;
    lastHmFontSize_ = hmFontSize_;
  }
  if (!secTex_ || currentSec_ != lastSec_ || secFontSize_ != lastSecFontSize_) {
    MemoryMonitor::getInstance().destroyTexture(secTex_);
    secTex_ = fontMgr_.renderText(renderer, currentSec_, themes.text, secFontSize_,
                                  &secW_, &secH_, true);
    lastSec_ = currentSec_;
    lastSecFontSize_ = secFontSize_;
  }
  if (hmTex_) {
    int dy = timeBaseY + (timeRowH - hmH_) / 2;
    SDL_Rect dst = {x_ + pad, dy, hmW_, hmH_};
    SDL_RenderCopy(renderer, hmTex_, nullptr, &dst);

    // SS superscript (aligned with top of HH:MM characters)
    if (secTex_) {
      // Large fonts have significant internal leading (top padding).
      // Nudge seconds down so their top is visually even with the big digits.
      int secY = dy + (hmH_ * 0.12f);
      SDL_Rect secDst = {x_ + pad + hmW_ + 2, secY, secW_, secH_};
      SDL_RenderCopy(renderer, secTex_, nullptr, &secDst);
    }
  }

  // --- Date (cyan, centered) ---
  if (!dateTex_ || currentDate_ != lastDate_ ||
      dateFontSize_ != lastDateFontSize_) {
    MemoryMonitor::getInstance().destroyTexture(dateTex_);
    SDL_Color dateColor = themes.info;
    dateTex_ = fontMgr_.renderText(renderer, currentDate_, dateColor, dateFontSize_,
                                   &dateW_, &dateH_);
    lastDate_ = currentDate_;
    lastDateFontSize_ = dateFontSize_;
  }
  if (dateTex_) {
    int dy = dateBaseY + (dateRowH - dateH_) / 2;
    int dx = x_ + (width_ - dateW_) / 2;
    SDL_Rect dst = {dx, dy, dateW_, dateH_};
    SDL_RenderCopy(renderer, dateTex_, nullptr, &dst);
  }

  // Star (presets) button — bottom-left of date row
  if (!editing_ && presetsRect_.w > 0) {
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &presetsRect_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &presetsRect_);
    if (!starTex_) {
      SDL_Color starCol = themes.accent;
      int ptSize = std::max(8, presetsRect_.h - 2);
      starTex_ = fontMgr_.renderText(renderer, "\xe2\x9c\xaf", starCol, ptSize,
                                     &starW_, &starH_);
    }
    if (starTex_) {
      SDL_Rect dst = {presetsRect_.x + (presetsRect_.w - starW_) / 2,
                      presetsRect_.y + (presetsRect_.h - starH_) / 2,
                      starW_, starH_};
      SDL_RenderCopy(renderer, starTex_, nullptr, &dst);
    }
  }

  // Editor overlay on top of everything
  if (editing_) {
    renderEditOverlay(renderer);
  }
}

void TimePanel::initPresets(AppConfig *cfg, std::function<void()> onApply) {
  presetsModal_ = std::make_unique<PresetsModal>(fontMgr_);
  presetsModal_->init(cfg, std::move(onApply));
}

bool TimePanel::isModalActive() const {
  return presetsModal_ && presetsModal_->isActive();
}

void TimePanel::renderModal(SDL_Renderer *renderer) {
  if (presetsModal_)
    presetsModal_->render(renderer);
}

void TimePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  callFontSize_ = static_cast<int>(cat->ptSize(FontStyle::LargeBold) * 0.8f);
  hmFontSize_ = cat->ptSize(FontStyle::LargeBold);
  secFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  dateFontSize_ = cat->ptSize(FontStyle::Fast);
  infoFontSize_ = cat->ptSize(FontStyle::Fast);

  int pad = std::max(4, static_cast<int>(w * 0.03f));
  gearSize_ = std::clamp(static_cast<int>(h * 0.10f), 8, 18);
  gearRect_ = {x_ + width_ - gearSize_ - pad, y_ + height_ - gearSize_ - pad,
               gearSize_, gearSize_};

  // Star button: 22×22 in the date row, left side
  int callRowH = h * 50 / 148;
  int infoRowH = h * 16 / 148;
  int timeRowH = h * 50 / 148;
  int dateBaseY = y_ + callRowH + infoRowH + timeRowH;
  int dateRowH  = y_ + h - dateBaseY;
  presetsRect_ = {x_ + 4, dateBaseY + (dateRowH - 22) / 2, 22, 22};

  destroyCache();
}

// --- Callsign Editor ---

void TimePanel::startEditing() {
  editing_ = true;
  editingBgColor_ = false; // always start in FG mode
  editInput_.setValue(callsign_);
  editInput_.setMaxLength(20);
  editInput_.setCursorToEnd();
  editInput_.setActive(true);
  // Sync selectedBgColorIdx_ from current callBgColor_
  selectedBgColorIdx_ = -1;
  if (callBgColor_.a > 0) {
    for (int i = 0; i < kNumColors; ++i) {
      if (kPalette[i].r == callBgColor_.r && kPalette[i].g == callBgColor_.g &&
          kPalette[i].b == callBgColor_.b) {
        selectedBgColorIdx_ = i;
        break;
      }
    }
  }
  SDL_StartTextInput();
}

void TimePanel::stopEditing(bool apply) {
  if (apply && !editInput_.getValue().empty()) {
    callsign_ = editInput_.getValue();
    callColor_ = kPalette[selectedColorIdx_];
    callBgColor_ = (selectedBgColorIdx_ >= 0) ? kPalette[selectedBgColorIdx_]
                                               : SDL_Color{0, 0, 0, 0};
    // Force callsign texture rebuild
    if (callTex_) {
      MemoryMonitor::getInstance().destroyTexture(callTex_);
    }
    // Persist change
    if (onConfigChanged_) {
      onConfigChanged_(callsign_, callColor_, callBgColor_);
    }
  }
  editing_ = false;
  SDL_StopTextInput();
}

bool TimePanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  // Delegate to presets modal when active
  if (presetsModal_ && presetsModal_->isActive()) {
    presetsModal_->onMouseUp(mx, my, mod);
    return true;
  }

  // Transport controls (pause / next) — check before gear
  if (!editing_) {
    if (pauseRect_.w > 0 && mx >= pauseRect_.x &&
        mx < pauseRect_.x + pauseRect_.w && my >= pauseRect_.y &&
        my < pauseRect_.y + pauseRect_.h) {
      if (onPauseRotation_)
        onPauseRotation_();
      return true;
    }
    if (nextRect_.w > 0 && mx >= nextRect_.x &&
        mx < nextRect_.x + nextRect_.w && my >= nextRect_.y &&
        my < nextRect_.y + nextRect_.h) {
      if (onNextRotation_)
        onNextRotation_();
      return true;
    }
  }

  // Gear icon click → request setup
  SDL_Rect hit = gearRect_;
  int margin = 5;
  hit.x -= margin;
  hit.y -= margin;
  hit.w += margin * 2;
  hit.h += margin * 2;

  if (!editing_ && mx >= hit.x && mx < hit.x + hit.w && my >= hit.y &&
      my < hit.y + hit.h) {
    setupRequested_ = true;
    return true;
  }

  // Star (presets) button
  if (!editing_ && presetsRect_.w > 0 && mx >= presetsRect_.x &&
      mx < presetsRect_.x + presetsRect_.w && my >= presetsRect_.y &&
      my < presetsRect_.y + presetsRect_.h) {
    if (presetsModal_)
      presetsModal_->open();
    return true;
  }

  if (editing_) {
    int pad = std::max(4, static_cast<int>(width_ * 0.03f));
    int editorFontSize = std::clamp(static_cast<int>(height_ * 0.18f), 12, 36);
    int textFieldH = editorFontSize + 12;
    int fieldY = y_ + pad;
    int tabRowH = 14;
    int tabY = fieldY + textFieldH + 4;
    int fieldW = width_ - 2 * pad;
    int tabW = (fieldW - 4) / 2;

    // Check FG tab click
    if (mx >= x_ + pad && mx < x_ + pad + tabW && my >= tabY &&
        my < tabY + tabRowH) {
      editingBgColor_ = false;
      return true;
    }
    // Check BG tab click
    if (mx >= x_ + pad + tabW + 4 && mx < x_ + pad + tabW + 4 + tabW &&
        my >= tabY && my < tabY + tabRowH) {
      editingBgColor_ = true;
      return true;
    }

    // Check palette swatches
    int paletteY = tabY + tabRowH + 4;
    int swatchSize = std::clamp(static_cast<int>(width_ * 0.08f), 16, 32);
    int gap = std::max(2, swatchSize / 6);
    int cols = 6;
    int numCells = editingBgColor_ ? kNumColors + 1 : kNumColors;

    for (int i = 0; i < numCells; ++i) {
      int col = i % cols;
      int row = i / cols;
      int sx = x_ + pad + col * (swatchSize + gap);
      int sy = paletteY + row * (swatchSize + gap);

      if (mx >= sx && mx < sx + swatchSize && my >= sy &&
          my < sy + swatchSize) {
        if (editingBgColor_) {
          selectedBgColorIdx_ = (i == 0) ? -1 : i - 1;
        } else {
          selectedColorIdx_ = i;
        }
        return true;
      }
    }

    // Click outside overlay → apply and close
    stopEditing(true);
    return true;
  }

  // Version click -> update request
  if (mx >= versionRect_.x && mx <= versionRect_.x + versionRect_.w &&
      my >= versionRect_.y && my <= versionRect_.y + versionRect_.h) {
    updateRequested_ = true;
    return true;
  }

  // Callsign area -> edit
  // ing: check if click is near the callsign text.
  // Clamp hit area to TimePanel bounds so a wide callsign + generous pad
  // cannot bleed into the adjacent pane to the right.
  int callRowH = height_ * 50 / 148;
  if (my >= y_ && my < y_ + callRowH && callW_ > 0) {
    int textX = x_ + (width_ - callW_) / 2;
    int pad = std::max(8, callW_ / 4);
    int hitL = std::max(x_, textX - pad);
    int hitR = std::min(x_ + width_, textX + callW_ + pad);
    if (mx >= hitL && mx < hitR) {
      startEditing();
      return true;
    }
  }
  return false;
}

bool TimePanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (presetsModal_ && presetsModal_->isActive())
    return presetsModal_->onKeyDown(key, mod);

  if (!editing_)
    return false;

  switch (key) {
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    stopEditing(true);
    return true;
  case SDLK_ESCAPE:
    stopEditing(false);
    return true;
  default:
    editInput_.onKeyDown(key, mod);
    return true;
  }
}

bool TimePanel::onTextInput(const char *text) {
  if (presetsModal_ && presetsModal_->isActive())
    return presetsModal_->onTextInput(text);

  if (!editing_)
    return false;
  editInput_.onTextInput(text);
  return true;
}

void TimePanel::renderEditOverlay(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  // Semi-transparent overlay
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  int pad = std::max(4, static_cast<int>(width_ * 0.03f));
  int editorFontSize = std::clamp(static_cast<int>(height_ * 0.18f), 12, 36);
  int textFieldH = editorFontSize + 12;

  // --- Text field background ---
  int fieldY = y_ + pad;
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_Rect fieldBg = {x_ + pad, fieldY, width_ - 2 * pad, textFieldH};
  SDL_RenderFillRect(renderer, &fieldBg);

  // Field border in selected color
  SDL_Color selColor = kPalette[selectedColorIdx_];
  SDL_SetRenderDrawColor(renderer, selColor.r, selColor.g, selColor.b, 255);
  SDL_RenderDrawRect(renderer, &fieldBg);

  // --- Draw edit text with cursor ---
  int textX = x_ + pad + 6;
  int textY = fieldY + 6;
  auto *cat = fontMgr_.catalog();

  const std::string &editStr = editInput_.getValue();
  if (!editStr.empty()) {
    cat->drawText(renderer, editStr, textX, textY, selColor,
                  FontStyle::MediumBold);
  }

  // Cursor: measure text up to cursorPos to find x offset.
  // Must pass bold=true — MediumBold is wider than plain, causing cursor
  // to appear mid-character if bold is omitted.
  int cursorX = textX;
  int cp = editInput_.getCursorPos();
  if (cp > 0) {
    std::string beforeCursor = editStr.substr(0, cp);
    int tw = fontMgr_.getLogicalWidth(beforeCursor,
                                      cat->ptSize(FontStyle::MediumBold), true);
    cursorX = textX + tw;
  }

  // Blinking cursor (visible 500ms on, 500ms off)
  auto ms = SDL_GetTicks();
  if ((ms / 500) % 2 == 0) {
    SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 255);
    SDL_RenderDrawLine(renderer, cursorX, fieldY + 3, cursorX,
                       fieldY + textFieldH - 3);
  }

  // --- FG / BG mode tabs ---
  int tabRowH = 14;
  int tabY = fieldY + textFieldH + 4;
  int fieldW = width_ - 2 * pad;
  int tabW = (fieldW - 4) / 2;
  SDL_Rect fgTab = {x_ + pad, tabY, tabW, tabRowH};
  SDL_Rect bgTab = {x_ + pad + tabW + 4, tabY, tabW, tabRowH};

  // FG tab: filled with current fg color
  {
    SDL_Color fc = kPalette[selectedColorIdx_];
    SDL_SetRenderDrawColor(renderer, fc.r / 3, fc.g / 3, fc.b / 3, 255);
    SDL_RenderFillRect(renderer, &fgTab);
    SDL_SetRenderDrawColor(renderer, fc.r, fc.g, fc.b, 255);
    SDL_RenderDrawRect(renderer, &fgTab);
    if (!editingBgColor_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 255);
      SDL_Rect outline = {fgTab.x - 1, fgTab.y - 1, fgTab.w + 2, fgTab.h + 2};
      SDL_RenderDrawRect(renderer, &outline);
    }
    cat->drawText(renderer, "TEXT", fgTab.x + fgTab.w / 2, fgTab.y + fgTab.h / 2,
                  themes.text, FontStyle::Tiny, true, false, true);
  }

  // BG tab: filled with current bg color (or themes.bg if none)
  {
    if (selectedBgColorIdx_ >= 0) {
      SDL_Color bc = kPalette[selectedBgColorIdx_];
      SDL_SetRenderDrawColor(renderer, bc.r / 3, bc.g / 3, bc.b / 3, 255);
      SDL_RenderFillRect(renderer, &bgTab);
      SDL_SetRenderDrawColor(renderer, bc.r, bc.g, bc.b, 255);
      SDL_RenderDrawRect(renderer, &bgTab);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                             255);
      SDL_RenderFillRect(renderer, &bgTab);
      SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g,
                             themes.textDim.b, 255);
      SDL_RenderDrawRect(renderer, &bgTab);
      // X mark for "none"
      SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g,
                             themes.textDim.b, 255);
      SDL_RenderDrawLine(renderer, bgTab.x + 3, bgTab.y + 3,
                         bgTab.x + bgTab.w - 4, bgTab.y + bgTab.h - 4);
      SDL_RenderDrawLine(renderer, bgTab.x + bgTab.w - 4, bgTab.y + 3,
                         bgTab.x + 3, bgTab.y + bgTab.h - 4);
    }
    if (editingBgColor_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 255);
      SDL_Rect outline = {bgTab.x - 1, bgTab.y - 1, bgTab.w + 2, bgTab.h + 2};
      SDL_RenderDrawRect(renderer, &outline);
    }
    cat->drawText(renderer, "BG", bgTab.x + bgTab.w / 2, bgTab.y + bgTab.h / 2,
                  themes.text, FontStyle::Tiny, true, false, true);
  }

  // --- Color palette ---
  int paletteY = tabY + tabRowH + 4;
  int swatchSize = std::clamp(static_cast<int>(width_ * 0.08f), 16, 32);
  int gap = std::max(2, swatchSize / 6);
  int cols = 6;

  // In BG mode: first cell is "none/clear" (dark with X), then palette colors.
  // In FG mode: all 12 palette colors.
  int numCells = editingBgColor_ ? kNumColors + 1 : kNumColors;
  for (int i = 0; i < numCells; ++i) {
    int col = i % cols;
    int row = i / cols;
    int sx = x_ + pad + col * (swatchSize + gap);
    int sy = paletteY + row * (swatchSize + gap);
    SDL_Rect swatch = {sx, sy, swatchSize, swatchSize};

    if (editingBgColor_ && i == 0) {
      // "None" cell
      SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                             255);
      SDL_RenderFillRect(renderer, &swatch);
      SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g,
                             themes.textDim.b, 255);
      SDL_RenderDrawLine(renderer, sx + 3, sy + 3, sx + swatchSize - 4,
                         sy + swatchSize - 4);
      SDL_RenderDrawLine(renderer, sx + swatchSize - 4, sy + 3, sx + 3,
                         sy + swatchSize - 4);
      if (selectedBgColorIdx_ == -1) {
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                               themes.accent.b, 255);
        SDL_Rect outline = {sx - 1, sy - 1, swatchSize + 2, swatchSize + 2};
        SDL_RenderDrawRect(renderer, &outline);
      }
    } else {
      int palIdx = editingBgColor_ ? i - 1 : i;
      SDL_Color c = kPalette[palIdx];
      SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
      SDL_RenderFillRect(renderer, &swatch);

      bool selected = editingBgColor_ ? (palIdx == selectedBgColorIdx_)
                                      : (palIdx == selectedColorIdx_);
      if (selected) {
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
        SDL_Rect outline = {sx - 1, sy - 1, swatchSize + 2, swatchSize + 2};
        SDL_RenderDrawRect(renderer, &outline);
      }
    }
  }

  // --- Hint text ---
  int hintY = paletteY + ((numCells + cols - 1) / cols) * (swatchSize + gap) + 2;
  if (hintY + 11 < y_ + height_) {
    cat->drawText(renderer, "Enter=Done  Esc=Cancel", x_ + pad, hintY,
                  themes.textDim, FontStyle::Caption);
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

std::vector<std::string> TimePanel::getActions() const {
  std::vector<std::string> actions = {"setup", "edit_callsign"};
  if (editing_) {
    actions.push_back("ok");
    actions.push_back("cancel");
    for (int i = 0; i < kNumColors; ++i) {
      actions.push_back("color_" + std::to_string(i));
    }
  }
  return actions;
}

SDL_Rect TimePanel::getActionRect(const std::string &action) const {
  if (action == "setup") {
    return gearRect_;
  }
  if (action == "edit_callsign") {
    int callRowH = height_ * 50 / 148;
    return {x_, y_, width_, callRowH};
  }

  if (editing_) {
    int pad = std::max(4, static_cast<int>(width_ * 0.03f));
    int editorFontSize = std::clamp(static_cast<int>(height_ * 0.18f), 12, 36);
    int textFieldH = editorFontSize + 12;

    if (action == "ok" || action == "cancel") {
      // For now, return the text field area as a general "interact" area
      return {x_ + pad, y_ + pad, width_ - 2 * pad, textFieldH};
    }

    if (action.find("color_") == 0) {
      int idx = StringUtils::safe_stoi(action.substr(6));
      int paletteY = y_ + pad + textFieldH + pad;
      int swatchSize = std::clamp(static_cast<int>(width_ * 0.08f), 16, 32);
      int gap = std::max(2, swatchSize / 6);
      int cols = 6;
      int col = idx % cols;
      int row = idx / cols;
      return {x_ + pad + col * (swatchSize + gap),
              paletteY + row * (swatchSize + gap), swatchSize, swatchSize};
    }
  }

  return {0, 0, 0, 0};
}

nlohmann::json TimePanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  j["callsign"] = callsign_;
  j["time_utc"] = currentHM_ + currentSec_;
  j["date"] = currentDate_;
  j["uptime"] = currentUptime_;
  j["editing"] = editing_;
  if (editing_) {
    j["editText"] = editInput_.getValue();
    j["cursorPos"] = editInput_.getCursorPos();
  }
  return j;
}

#include "WidgetRegistry.h"
REGISTER_WIDGET("time", "Time/Clock", false, false, {
  return std::make_unique<TimePanel>(0, 0, 0, 0, deps.fontMgr, deps.texMgr, deps.appCfg.callsign);
})
