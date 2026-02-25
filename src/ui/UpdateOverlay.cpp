#include "UpdateOverlay.h"
#include <algorithm>

UpdateOverlay::UpdateOverlay(int x, int y, int w, int h, FontManager &fontMgr,
                             UpdateChecker &updateChecker)
    : Widget(x, y, w, h), fontMgr_(fontMgr), updateChecker_(updateChecker) {
  recalcLayout();
}

UpdateOverlay::~UpdateOverlay() {}

void UpdateOverlay::update() {
  std::string notes = updateChecker_.releaseNotes();
  if (notes != lastNotes_ || width_ != lastWidth_) {
    lastNotes_ = notes;
    lastWidth_ = width_;
    wrappedLines_.clear();

    int scrollW = 12; // Slimmer scrollbar
    int maxW = width_ - 2 * padding_ - scrollW;

    size_t start = 0;
    while (start < notes.length()) {
      size_t end = notes.find('\n', start);
      std::string rawLine =
          notes.substr(start, (end == std::string::npos) ? std::string::npos
                                                         : (end - start));

      // Parse Markdown properties
      int lineSize = bodySize_;
      SDL_Color lineColor = {255, 255, 255, 255};
      bool isBold = false;
      int indent = 0;
      std::string text = rawLine;

      // Robust trim (handles leading BOM/control chars and CRLF)
      auto robustTrim = [](std::string s) {
        // Strip leading non-printables (BOM, control chars)
        while (!s.empty() && static_cast<unsigned char>(s[0]) < 33) {
          s.erase(0, 1);
        }
        // Trim trailing whitespace/CRLF
        size_t last = s.find_last_not_of(" \t\r\n\v\f");
        if (last != std::string::npos)
          s.erase(last + 1);
        return s;
      };

      text = robustTrim(rawLine);

      if (!text.empty()) {
        auto startsWith = [](const std::string &s, const std::string &pre) {
          return s.compare(0, pre.length(), pre) == 0;
        };

        if (startsWith(text, "### ")) {
          lineSize = bodySize_;
          isBold = true;
          text = text.substr(4);
        } else if (startsWith(text, "## ")) {
          lineSize = static_cast<int>(bodySize_ * 1.25f);
          isBold = true;
          text = text.substr(3);
        } else if (startsWith(text, "# ")) {
          lineSize = static_cast<int>(bodySize_ * 1.5f);
          lineColor = {0, 200, 255, 255}; // Cyan
          isBold = true;
          text = text.substr(2);
        } else if (startsWith(text, "* ") || startsWith(text, "- ")) {
          indent = 20;
          text = "• " + text.substr(2);
        }

        // Strip ** markers (until we support mid-line bold)
        size_t bpos;
        while ((bpos = text.find("**")) != std::string::npos) {
          text.erase(bpos, 2);
        }
      }

      if (text.empty()) {
        wrappedLines_.push_back({"", lineSize, lineColor, isBold, indent});
      } else {
        // Wrap this styled line
        std::string currentLine;
        std::string word;
        int effectiveMaxW = maxW - indent;

        auto addWrappedLine = [&](const std::string &lt) {
          wrappedLines_.push_back({lt, lineSize, lineColor, isBold, indent});
        };

        for (size_t i = 0; i <= text.length(); ++i) {
          if (i == text.length() || text[i] == ' ') {
            if (word.empty() && i < text.length())
              continue;

            std::string testLine =
                currentLine.empty() ? word : currentLine + " " + word;
            if (fontMgr_.getLogicalWidth(testLine, lineSize, isBold) >
                effectiveMaxW) {
              if (!currentLine.empty()) {
                addWrappedLine(currentLine);
                currentLine = word;
              } else {
                // Word itself is longer than effectiveMaxW
                addWrappedLine(word);
                currentLine = "";
              }
            } else {
              currentLine = testLine;
            }
            word = "";
          } else {
            word += text[i];
          }
        }
        if (!currentLine.empty()) {
          addWrappedLine(currentLine);
        }
      }

      if (end == std::string::npos)
        break;
      start = end + 1;
    }

    // Calculate total height based on variable line sizes
    int totalH = 0;
    for (const auto &wl : wrappedLines_) {
      int lh = static_cast<int>(wl.size * 1.8f);
      totalH += lh;
    }
    maxScroll_ = std::max(0, totalH - notesArea_.h);
    scrollPos_ = 0;
  }
}

void UpdateOverlay::recalcLayout() {
  titleSize_ = std::clamp(static_cast<int>(height_ * 0.06f), 20, 32);
  bodySize_ = std::clamp(static_cast<int>(height_ * 0.035f), 13, 16);
  btnSize_ = std::clamp(static_cast<int>(height_ * 0.04f), 14, 20);
  padding_ = 12; // Aggressive reduction to maximize text space

  int btnW = std::min(120, (width_ - 4 * padding_) / 3);
  int btnH = btnSize_ + 16;
  int headerH =
      titleSize_ + bodySize_ + padding_ * 3; // More room for version line
  int footerH = btnH + padding_ * 2;

  notesArea_ = {x_ + padding_, y_ + headerH, width_ - 2 * padding_,
                height_ - headerH - footerH};

  int btnY = y_ + height_ - padding_ - btnH;
  skipBtn_ = {x_ + padding_, btnY, btnW, btnH};
  notNowBtn_ = {x_ + width_ / 2 - btnW / 2, btnY, btnW, btnH};
  updateBtn_ = {x_ + width_ - padding_ - btnW, btnY, btnW, btnH};
}

void UpdateOverlay::render(SDL_Renderer *renderer) {
  // Semi-transparent dimming of backbuffer
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_Rect screen = {0, 0, 2048, 2048}; // Sufficiently large
  SDL_RenderFillRect(renderer, &screen);

  // Modal background
  SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255);
  SDL_Rect modal = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &modal);
  SDL_SetRenderDrawColor(renderer, 60, 70, 90, 255);
  SDL_RenderDrawRect(renderer, &modal);

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color cyan = {0, 200, 255, 255};

  // Header
  int ty = y_ + padding_;
  fontMgr_.drawText(renderer, "Updates Available", x_ + width_ / 2, ty, cyan,
                    titleSize_, true, true);
  ty += titleSize_ + 4;

  std::string info =
      std::string(HAMCLOCK_VERSION) + " -> " + updateChecker_.latestVersion();
  info +=
      " (" + updateChecker_.arch() + " " + updateChecker_.installType() + ")";
  fontMgr_.drawText(renderer, info, x_ + width_ / 2, ty, white, bodySize_,
                    false, true);

  // Release Notes Area
  SDL_RenderSetClipRect(renderer, &notesArea_);
  int ly = notesArea_.y - scrollPos_;
  for (const auto &wl : wrappedLines_) {
    int lh = static_cast<int>(wl.size * 1.8f);
    if (ly + wl.size > notesArea_.y && ly < notesArea_.y + notesArea_.h) {
      fontMgr_.drawText(renderer, wl.text, notesArea_.x + wl.indent, ly,
                        wl.color, wl.size, wl.bold);
    }
    ly += lh;
  }
  SDL_RenderSetClipRect(renderer, nullptr);

  // Scrollbar if needed
  if (maxScroll_ > 0) {
    int sbW = 6;
    int sbX = notesArea_.x + notesArea_.w - sbW;
    SDL_Rect track = {sbX, notesArea_.y, sbW, notesArea_.h};
    SDL_SetRenderDrawColor(renderer, 40, 45, 55, 255);
    SDL_RenderFillRect(renderer, &track);

    float visiblePct =
        std::min(1.0f, (float)notesArea_.h / (maxScroll_ + notesArea_.h));
    int thumbH = std::max(20, (int)(notesArea_.h * visiblePct));
    int thumbY = notesArea_.y + (int)((float)scrollPos_ / maxScroll_ *
                                      (notesArea_.h - thumbH));
    SDL_Rect thumb = {sbX, thumbY, sbW, thumbH};
    SDL_SetRenderDrawColor(renderer, 100, 110, 130, 255);
    SDL_RenderFillRect(renderer, &thumb);
  }

  // Buttons
  auto drawBtn = [&](SDL_Rect r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
    SDL_RenderDrawRect(renderer, &r);
    fontMgr_.drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, white,
                      btnSize_, false, true);
  };

  drawBtn(skipBtn_, "Skip", {60, 60, 70, 255});
  drawBtn(notNowBtn_, "Not Now", {40, 45, 55, 255});
  drawBtn(updateBtn_, "Update", {30, 80, 40, 255});
}

void UpdateOverlay::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
  lastWidth_ = 0; // Trigger re-wrap
}

bool UpdateOverlay::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  auto inRect = [&](SDL_Rect r) {
    return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
  };

  if (inRect(skipBtn_)) {
    result_ = Result::Skip;
    return true;
  }
  if (inRect(notNowBtn_)) {
    result_ = Result::NotNow;
    return true;
  }
  if (inRect(updateBtn_)) {
    result_ = Result::Update;
    return true;
  }

  return true; // Modal consumes all clicks
}

bool UpdateOverlay::onMouseWheel(int scrollY) {
  if (maxScroll_ > 0) {
    scrollPos_ = std::clamp(scrollPos_ - scrollY * 30, 0, maxScroll_);
    return true;
  }
  return false;
}
