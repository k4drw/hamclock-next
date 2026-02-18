#include "TimePanel.h"
#include "../core/Astronomy.h"
#include "../core/MemoryMonitor.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
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

static constexpr const char *kVersion = "V" HAMCLOCK_VERSION;
static constexpr Uint32 kInfoRotateMs = 3000;

std::string getSystemUptime() {
#ifdef _WIN32
  // TODO: GetTickCount64() logic for Windows
  return "Up --";
#else
  std::FILE *f = std::fopen("/proc/uptime", "r");
  if (!f)
    return "Up ?";
  double sec = 0;
  if (std::fscanf(f, "%lf", &sec) != 1)
    sec = 0;
  std::fclose(f);
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
#endif
}

std::string getCpuTemp() {
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
  std::snprintf(buf, sizeof(buf), "CPU %.0fC", milliC / 1000.0);
  return buf;
#endif
}

std::string getDiskUsage() {
#ifdef _WIN32
  return "Disk --";
#else
  struct statvfs stat;
  if (statvfs("/", &stat) != 0)
    return "Disk --";
  double total = static_cast<double>(stat.f_blocks) * stat.f_frsize;
  double avail = static_cast<double>(stat.f_bavail) * stat.f_frsize;
  int pct = (total > 0) ? static_cast<int>(100.0 * (1.0 - avail / total)) : 0;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "Disk %d%%", pct);
  return buf;
#endif
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
}

void TimePanel::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", utc.tm_hour, utc.tm_min);
  currentHM_ = buf;

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
    infoTexts_[0] = getCpuTemp();
    infoTexts_[1] = getDiskUsage();
    infoTexts_[2] = getLocalIP();
  }
  // Seed on first call
  if (infoTexts_[0].empty()) {
    infoTexts_[0] = getCpuTemp();
    infoTexts_[1] = getDiskUsage();
    infoTexts_[2] = getLocalIP();
  }
}

void TimePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

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

  // --- Gear icon (bottom-right, setup trigger) ---
  if (!editing_) {
    float gcx = gearRect_.x + gearRect_.w / 2.0f;
    float gcy = gearRect_.y + gearRect_.h / 2.0f;
    float r = gearSize_ / 2.0f;
    SDL_Color gearColor = {140, 140, 140, 255};
    SDL_Color bgColor = {10, 10, 20, 255};
    RenderUtils::drawGear(renderer, gcx, gcy, r, gearColor, bgColor);
  }

  // --- Info bar: uptime (left), rotating center, version (right) ---
  {
    SDL_Color gray = themes.textDim;
    int infoY = infoBaseY + (infoRowH - infoFontSize_) / 2;
    TTF_Font *infoFont = fontMgr_.getFont(infoFontSize_);
    if (infoFont) {
      int tw = 0, th = 0;

      // Left: uptime
      fontMgr_.drawText(renderer, currentUptime_, x_ + pad, infoY, gray,
                        infoFontSize_);

      // Rotating info (shifted slightly right to give more room for uptime)
      const std::string &centerText = infoTexts_[infoRotateIdx_];
      TTF_SizeUTF8(infoFont, centerText.c_str(), &tw, &th);
      fontMgr_.drawText(renderer, centerText, x_ + (width_ - tw) * 0.58f, infoY,
                        gray, infoFontSize_);

      // Right: version
      TTF_SizeUTF8(infoFont, kVersion, &tw, &th);
      fontMgr_.drawText(renderer, kVersion, x_ + width_ - pad - tw, infoY, gray,
                        infoFontSize_);
    }
  }

  // --- Time: HH:MM (large, white) + SS (superscript, gray) ---
  if (!hmTex_ || currentHM_ != lastHM_ || hmFontSize_ != lastHmFontSize_) {
    MemoryMonitor::getInstance().destroyTexture(hmTex_);
    SDL_Color white = {255, 255, 255, 255};
    hmTex_ = fontMgr_.renderText(renderer, currentHM_, white, hmFontSize_,
                                 &hmW_, &hmH_);
    lastHM_ = currentHM_;
    lastHmFontSize_ = hmFontSize_;
  }
  if (!secTex_ || currentSec_ != lastSec_ || secFontSize_ != lastSecFontSize_) {
    MemoryMonitor::getInstance().destroyTexture(secTex_);
    SDL_Color white = {255, 255, 255, 255};
    secTex_ = fontMgr_.renderText(renderer, currentSec_, white, secFontSize_,
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
    SDL_Color cyan = {0, 200, 255, 255};
    dateTex_ = fontMgr_.renderText(renderer, currentDate_, cyan, dateFontSize_,
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

  // Editor overlay on top of everything
  if (editing_) {
    renderEditOverlay(renderer);
  }
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

  destroyCache();
}

// --- Callsign Editor ---

void TimePanel::startEditing() {
  editing_ = true;
  editText_ = callsign_;
  cursorPos_ = static_cast<int>(editText_.size());
  SDL_StartTextInput();
}

void TimePanel::stopEditing(bool apply) {
  if (apply && !editText_.empty()) {
    callsign_ = editText_;
    callColor_ = kPalette[selectedColorIdx_];
    // Force callsign texture rebuild
    if (callTex_) {
      MemoryMonitor::getInstance().destroyTexture(callTex_);
    }
    // Persist change
    if (onConfigChanged_) {
      onConfigChanged_(callsign_, callColor_);
    }
  }
  editing_ = false;
  SDL_StopTextInput();
}

bool TimePanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
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

  if (editing_) {
    // Check if clicking a color swatch
    int pad = std::max(4, static_cast<int>(width_ * 0.03f));
    int editorFontSize = std::clamp(static_cast<int>(height_ * 0.18f), 12, 36);
    int textFieldH = editorFontSize + 12;

    // Layout: text field at top, then color palette below
    int fieldY = y_ + pad;
    int paletteY = fieldY + textFieldH + pad;

    int swatchSize = std::clamp(static_cast<int>(width_ * 0.08f), 16, 32);
    int gap = std::max(2, swatchSize / 6);
    int cols = 6;

    for (int i = 0; i < kNumColors; ++i) {
      int col = i % cols;
      int row = i / cols;
      int sx = x_ + pad + col * (swatchSize + gap);
      int sy = paletteY + row * (swatchSize + gap);

      if (mx >= sx && mx < sx + swatchSize && my >= sy &&
          my < sy + swatchSize) {
        selectedColorIdx_ = i;
        return true;
      }
    }

    // Click outside the overlay area → apply and close
    stopEditing(true);
    return true;
  }

  // Not editing: check if click is near the callsign text
  int callRowH = height_ * 42 / 148;
  if (my >= y_ && my < y_ + callRowH && callW_ > 0) {
    int textX = x_ + (width_ - callW_) / 2;
    int pad = std::max(8, callW_ / 4);
    if (mx >= textX - pad && mx < textX + callW_ + pad) {
      startEditing();
      return true;
    }
  }
  return false;
}

bool TimePanel::onKeyDown(SDL_Keycode key, Uint16 /*mod*/) {
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
  case SDLK_BACKSPACE:
    if (cursorPos_ > 0) {
      editText_.erase(cursorPos_ - 1, 1);
      --cursorPos_;
    }
    return true;
  case SDLK_DELETE:
    if (cursorPos_ < static_cast<int>(editText_.size())) {
      editText_.erase(cursorPos_, 1);
    }
    return true;
  case SDLK_LEFT:
    if (cursorPos_ > 0)
      --cursorPos_;
    return true;
  case SDLK_RIGHT:
    if (cursorPos_ < static_cast<int>(editText_.size()))
      ++cursorPos_;
    return true;
  case SDLK_HOME:
    cursorPos_ = 0;
    return true;
  case SDLK_END:
    cursorPos_ = static_cast<int>(editText_.size());
    return true;
  default:
    return true; // consume all keys while editing
  }
}

bool TimePanel::onTextInput(const char *text) {
  if (!editing_)
    return false;

  // Limit callsign length to 20 characters
  if (editText_.size() >= 20)
    return true;

  editText_.insert(cursorPos_, text);
  cursorPos_ += static_cast<int>(std::strlen(text));
  return true;
}

void TimePanel::renderEditOverlay(SDL_Renderer *renderer) {
  // Semi-transparent dark overlay over the entire panel
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  int pad = std::max(4, static_cast<int>(width_ * 0.03f));
  int editorFontSize = std::clamp(static_cast<int>(height_ * 0.18f), 12, 36);
  int textFieldH = editorFontSize + 12;

  // --- Text field background ---
  int fieldY = y_ + pad;
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_Rect fieldBg = {x_ + pad, fieldY, width_ - 2 * pad, textFieldH};
  SDL_RenderFillRect(renderer, &fieldBg);

  // Field border in selected color
  SDL_Color selColor = kPalette[selectedColorIdx_];
  SDL_SetRenderDrawColor(renderer, selColor.r, selColor.g, selColor.b, 255);
  SDL_RenderDrawRect(renderer, &fieldBg);

  // --- Draw edit text with cursor ---
  int textX = x_ + pad + 6;
  int textY = fieldY + 6;

  if (!editText_.empty()) {
    fontMgr_.drawText(renderer, editText_, textX, textY, selColor,
                      editorFontSize);
  }

  // Cursor: measure text up to cursorPos_ to find x offset
  int cursorX = textX;
  if (cursorPos_ > 0) {
    std::string beforeCursor = editText_.substr(0, cursorPos_);
    TTF_Font *font = fontMgr_.getFont(editorFontSize);
    if (font) {
      int tw = 0, th = 0;
      TTF_SizeText(font, beforeCursor.c_str(), &tw, &th);
      cursorX = textX + tw;
    }
  }

  // Blinking cursor (visible 500ms on, 500ms off)
  auto ms = SDL_GetTicks();
  if ((ms / 500) % 2 == 0) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, cursorX, fieldY + 3, cursorX,
                       fieldY + textFieldH - 3);
  }

  // --- Color palette ---
  int paletteY = fieldY + textFieldH + pad;
  int swatchSize = std::clamp(static_cast<int>(width_ * 0.08f), 16, 32);
  int gap = std::max(2, swatchSize / 6);
  int cols = 6;

  for (int i = 0; i < kNumColors; ++i) {
    int col = i % cols;
    int row = i / cols;
    int sx = x_ + pad + col * (swatchSize + gap);
    int sy = paletteY + row * (swatchSize + gap);

    SDL_Color c = kPalette[i];
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_Rect swatch = {sx, sy, swatchSize, swatchSize};
    SDL_RenderFillRect(renderer, &swatch);

    // Highlight selected swatch with white border
    if (i == selectedColorIdx_) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_Rect outline = {sx - 1, sy - 1, swatchSize + 2, swatchSize + 2};
      SDL_RenderDrawRect(renderer, &outline);
    }
  }

  // --- Hint text ---
  int hintY =
      paletteY + ((kNumColors + cols - 1) / cols) * (swatchSize + gap) + pad;
  if (hintY + 14 < y_ + height_) {
    int hintSize = std::clamp(static_cast<int>(height_ * 0.08f), 8, 16);
    SDL_Color gray = {140, 140, 140, 255};
    fontMgr_.drawText(renderer, "Enter=OK  Esc=Cancel", x_ + pad, hintY, gray,
                      hintSize);
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
    int callRowH = height_ * 42 / 148;
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
    j["editText"] = editText_;
    j["cursorPos"] = cursorPos_;
  }
  return j;
}
