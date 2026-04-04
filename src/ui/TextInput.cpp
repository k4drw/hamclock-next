#include "TextInput.h"
#include <SDL.h>
#include <algorithm>
#include <cstring>

// ─── Helpers ─────────────────────────────────────────────────────────────────

int TextInput::clamp(int pos) const {
  if (pos < 0)
    return 0;
  int len = static_cast<int>(text_.size());
  return pos > len ? len : pos;
}

void TextInput::moveCursor(int newPos, bool extendSelection) {
  cursorPos_ = clamp(newPos);
  if (!extendSelection)
    selectionAnchor_ = cursorPos_;
}

int TextInput::findNextWord(int start) const {
  int len = static_cast<int>(text_.size());
  if (start >= len)
    return len;
  int i = start;
  // If at space, skip to start of word
  while (i < len && !std::isalnum((unsigned char)text_[i]))
    i++;
  // Skip the word
  while (i < len && std::isalnum((unsigned char)text_[i]))
    i++;
  return i;
}

int TextInput::findPrevWord(int start) const {
  if (start <= 0)
    return 0;
  int i = start - 1;
  // If at space, skip to end of previous word
  while (i > 0 && !std::isalnum((unsigned char)text_[i]))
    i--;
  // Skip backwards through word
  while (i > 0 && std::isalnum((unsigned char)text_[i]))
    i--;
  // If we stopped because we hit a non-alphanumeric, move forward one to start of word
  if (i < static_cast<int>(text_.size()) && !std::isalnum((unsigned char)text_[i]))
    i++;
  return i;
}

bool TextInput::deleteSelection() {
  if (cursorPos_ == selectionAnchor_)
    return false;
  int start = std::min(cursorPos_, selectionAnchor_);
  int end = std::max(cursorPos_, selectionAnchor_);
  text_.erase(start, end - start);
  cursorPos_ = start;
  selectionAnchor_ = cursorPos_;
  return true;
}

// ─── Value ───────────────────────────────────────────────────────────────────

void TextInput::setValue(const std::string &s) {
  if (text_ == s)
    return;
  text_ = s;
  cursorPos_ = clamp(cursorPos_);
  selectionAnchor_ = cursorPos_;
}

void TextInput::clear() {
  text_.clear();
  cursorPos_ = 0;
  selectionAnchor_ = 0;
}

// ─── Selection ───────────────────────────────────────────────────────────────

std::string TextInput::getSelectedText() const {
  if (cursorPos_ == selectionAnchor_)
    return {};
  int start = std::min(cursorPos_, selectionAnchor_);
  int end = std::max(cursorPos_, selectionAnchor_);
  return text_.substr(start, end - start);
}

// ─── Cursor helpers ──────────────────────────────────────────────────────────

void TextInput::setCursorToEnd() {
  cursorPos_ = static_cast<int>(text_.size());
  selectionAnchor_ = cursorPos_;
}

void TextInput::setCursorPos(int pos) {
  cursorPos_ = clamp(pos);
  selectionAnchor_ = cursorPos_;
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

bool TextInput::onKeyDown(SDL_Keycode key, Uint16 mod) {
  bool shift = (mod & (KMOD_SHIFT)) != 0;
  bool ctrl = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;

  switch (key) {
  case SDLK_BACKSPACE:
    if (deleteSelection()) {
      return true;
    }
    if (ctrl) {
      int prev = findPrevWord(cursorPos_);
      if (prev < cursorPos_) {
        text_.erase(prev, cursorPos_ - prev);
        cursorPos_ = prev;
        selectionAnchor_ = cursorPos_;
      }
    } else if (cursorPos_ > 0) {
      text_.erase(cursorPos_ - 1, 1);
      --cursorPos_;
      selectionAnchor_ = cursorPos_;
    }
    return true;

  case SDLK_DELETE:
    if (deleteSelection()) {
      return true;
    }
    if (ctrl) {
      int next = findNextWord(cursorPos_);
      if (next > cursorPos_) {
        text_.erase(cursorPos_, next - cursorPos_);
        selectionAnchor_ = cursorPos_;
      }
    } else if (cursorPos_ < static_cast<int>(text_.size())) {
      text_.erase(cursorPos_, 1);
      selectionAnchor_ = cursorPos_;
    }
    return true;

  case SDLK_LEFT:
    if (ctrl)
      moveCursor(findPrevWord(cursorPos_), shift);
    else
      moveCursor(cursorPos_ - 1, shift);
    return true;

  case SDLK_RIGHT:
    if (ctrl)
      moveCursor(findNextWord(cursorPos_), shift);
    else
      moveCursor(cursorPos_ + 1, shift);
    return true;

  case SDLK_HOME:
    moveCursor(0, shift);
    return true;

  case SDLK_END:
    moveCursor(static_cast<int>(text_.size()), shift);
    return true;

  case SDLK_a:
    if (ctrl) {
      selectionAnchor_ = 0;
      cursorPos_ = static_cast<int>(text_.size());
    }
    return true;

  case SDLK_c:
    if (ctrl && hasSelection()) {
      std::string sel = getSelectedText();
      SDL_SetClipboardText(sel.c_str());
    }
    return true;

  case SDLK_x:
    if (ctrl && hasSelection()) {
      std::string sel = getSelectedText();
      SDL_SetClipboardText(sel.c_str());
      deleteSelection();
    }
    return true;

  case SDLK_v:
    if (ctrl) {
      char *clip = SDL_GetClipboardText();
      if (clip) {
        onTextInput(clip);
        SDL_free(clip);
      }
    }
    return true;

  default:
    return false; // Not consumed — let caller handle (Tab, Esc, Enter, etc.)
  }
}

bool TextInput::onTextInput(const char *text) {
  if (!text || !text[0])
    return true;

  deleteSelection();

  int len = static_cast<int>(strlen(text));
  if (maxLen_ > 0 && static_cast<int>(text_.size()) + len > maxLen_) {
    // Insert only as many characters as fit
    int avail = maxLen_ - static_cast<int>(text_.size());
    if (avail <= 0)
      return true;
    text_.insert(cursorPos_, text, avail);
    cursorPos_ += avail;
  } else {
    text_.insert(cursorPos_, text);
    cursorPos_ += len;
  }
  selectionAnchor_ = cursorPos_;
  return true;
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

bool TextInput::onMouseDown(int mx, int my, int clicks, FontManager &fontMgr,
                            int fieldX, int fieldY, int fieldW, int fieldH,
                            FontStyle style, int textPad) {
  if (mx < fieldX || mx >= fieldX + fieldW || my < fieldY ||
      my >= fieldY + fieldH)
    return false;

  active_ = true;

  if (clicks == 2) {
    selectionAnchor_ = 0;
    cursorPos_ = static_cast<int>(text_.size());
    mouseDown_ = false; // Disable drag on double-click
    return true;
  }

  auto *cat = fontMgr.catalog();
  int fontSize = cat->ptSize(style);
  bool bold = FontCatalog::isBold(style);
  int textStartX = fieldX + textPad;
  int relX = mx - textStartX;

  if (relX <= 0) {
    moveCursor(0, false);
    return true;
  }

  if (text_.empty()) {
    moveCursor(0, false);
    return true;
  }

  int fullW = fontMgr.getLogicalWidth(text_, fontSize, bold);
  if (relX >= fullW) {
    moveCursor(static_cast<int>(text_.size()), false);
    return true;
  }

  int bestPos = 0;
  int bestDist = std::abs(relX);
  for (int i = 1; i <= static_cast<int>(text_.size()); ++i) {
    int w = fontMgr.getLogicalWidth(text_.substr(0, i), fontSize, bold);
    int dist = std::abs(relX - w);
    if (dist < bestDist) {
      bestDist = dist;
      bestPos = i;
    } else {
      break;
    }
  }
  moveCursor(bestPos, false);
  mouseDown_ = true;
  return true;
}

void TextInput::onMouseMove(int mx, FontManager &fontMgr, int fieldX,
                            int fieldW, FontStyle style, int textPad) {
  if (!mouseDown_ || text_.empty())
    return;

  auto *cat = fontMgr.catalog();
  int fontSize = cat->ptSize(style);
  bool bold = FontCatalog::isBold(style);
  int textStartX = fieldX + textPad;
  int relX = mx - textStartX;

  int newPos = 0;
  if (relX > 0) {
    int fullW = fontMgr.getLogicalWidth(text_, fontSize, bold);
    if (relX >= fullW) {
      newPos = static_cast<int>(text_.size());
    } else {
      int bestDist = std::abs(relX);
      for (int i = 1; i <= static_cast<int>(text_.size()); ++i) {
        int w = fontMgr.getLogicalWidth(text_.substr(0, i), fontSize, bold);
        int dist = std::abs(relX - w);
        if (dist < bestDist) {
          bestDist = dist;
          newPos = i;
        } else {
          break;
        }
      }
    }
  }
  moveCursor(newPos, true); // extend selection from anchor
}

void TextInput::onMouseUp() { mouseDown_ = false; }

// ─── Render ──────────────────────────────────────────────────────────────────

void TextInput::render(SDL_Renderer *renderer, FontManager &fontMgr, int fieldX,
                       int fieldY, int fieldW, int fieldH, FontStyle fieldStyle,
                       int textPad, bool active, bool valid,
                       SDL_Color activeBorder, SDL_Color inactiveBorder,
                       SDL_Color validColor, SDL_Color textColor,
                       SDL_Color placeholderColor,
                       const std::string &placeholder,
                       const SDL_Color *bgColor) const {
  SDL_Color border = active ? activeBorder : inactiveBorder;
  auto *cat = fontMgr.catalog();

  // Background
  if (bgColor) {
    SDL_SetRenderDrawColor(renderer, bgColor->r, bgColor->g, bgColor->b, bgColor->a);
  } else {
    // All production callers supply bgColor from their ThemeColors; this
    // branch is a defensive fallback and is not reached in normal use.
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
  }
  SDL_Rect rect = {fieldX, fieldY, fieldW, fieldH};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (active) {
    SDL_Rect glow = {fieldX - 1, fieldY - 1, fieldW + 2, fieldH + 2};
    SDL_SetRenderDrawColor(renderer, border.r / 2, border.g / 2, border.b / 2,
                           255);
    SDL_RenderDrawRect(renderer, &glow);
  }

  // Clip to field interior
  SDL_Rect clip = {fieldX + 2, fieldY + 2, fieldW - 4, fieldH - 4};
  SDL_RenderSetClipRect(renderer, &clip);

  // Selection highlight
  if (active && cursorPos_ != selectionAnchor_ && !text_.empty()) {
    int start = std::min(cursorPos_, selectionAnchor_);
    int end = std::max(cursorPos_, selectionAnchor_);
    int selX = fieldX + textPad;
    if (start > 0) {
      selX += fontMgr.getLogicalWidth(text_.substr(0, start),
                                      cat->ptSize(fieldStyle),
                                      FontCatalog::isBold(fieldStyle));
    }
    int selW = fontMgr.getLogicalWidth(text_.substr(start, end - start),
                                       cat->ptSize(fieldStyle),
                                       FontCatalog::isBold(fieldStyle));
    SDL_Rect selRect = {selX, fieldY + 4, selW, fieldH - 8};
    SDL_SetRenderDrawColor(renderer, activeBorder.r, activeBorder.g, activeBorder.b, 80);
    SDL_RenderFillRect(renderer, &selRect);
  }

  // Text or placeholder
  if (!text_.empty()) {
    SDL_Color color = valid ? validColor : textColor;
    cat->drawText(renderer, text_, fieldX + textPad, fieldY + textPad, color,
                  fieldStyle);
  } else if (!active && !placeholder.empty()) {
    cat->drawText(renderer, placeholder, fieldX + textPad, fieldY + textPad,
                  placeholderColor, fieldStyle);
  }

  SDL_RenderSetClipRect(renderer, nullptr);

  // Cursor
  if (active) {
    int cursorX = fieldX + textPad;
    if (cursorPos_ > 0 && !text_.empty()) {
      cursorX += fontMgr.getLogicalWidth(text_.substr(0, cursorPos_),
                                         cat->ptSize(fieldStyle),
                                         FontCatalog::isBold(fieldStyle));
    }
    if ((SDL_GetTicks() / 500) % 2 == 0) {
      SDL_SetRenderDrawColor(renderer, textColor.r, textColor.g, textColor.b, 255);
      SDL_RenderDrawLine(renderer, cursorX, fieldY + 4, cursorX,
                         fieldY + fieldH - 4);
    }
  }
}
