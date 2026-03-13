#pragma once

#include "FontCatalog.h"
#include "FontManager.h"
#include <SDL.h>
#include <string>

// Reusable single-line text input widget.
// Caller owns layout; TextInput handles text state, cursor, selection,
// keyboard, clipboard, and rendering of a single field box.
class TextInput {
public:
    TextInput() = default;

    // Value access
    void setValue(const std::string &s);
    const std::string &getValue() const { return text_; }
    void clear();

    // Optional per-instance max length (0 = unlimited)
    void setMaxLength(int n) { maxLen_ = n; }

    // Event handlers — return true if event was consumed
    // mod: SDL modifier bitmask (Uint16)
    bool onKeyDown(SDL_Keycode key, Uint16 mod);
    bool onTextInput(const char *text);

    // Mouse click → place cursor at click position
    bool onMouseDown(int mx, int my, int clicks, FontManager &fontMgr,
                     int fieldX, int fieldY, int fieldW, int fieldH,
                     FontStyle style, int textPad);

    // Mouse drag → extend selection while button held; call after onMouseDown
    void onMouseMove(int mx, FontManager &fontMgr,
                     int fieldX, int fieldW, FontStyle style, int textPad);

    // Mouse release → end drag
    void onMouseUp();

    // Render the field box at (fieldX, fieldY, fieldW, fieldH).
    // Does NOT advance y — caller controls layout.
    void render(SDL_Renderer *renderer, FontManager &fontMgr,
                int fieldX, int fieldY, int fieldW, int fieldH,
                FontStyle fieldStyle, int textPad,
                bool active, bool valid,
                SDL_Color activeBorder, SDL_Color inactiveBorder,
                SDL_Color validColor, SDL_Color textColor,
                SDL_Color placeholderColor,
                const std::string &placeholder = "",
                const SDL_Color *bgColor = nullptr) const;

    // Focus
    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }

    // Selection
    bool hasSelection() const { return cursorPos_ != selectionAnchor_; }
    std::string getSelectedText() const;

    // Direct cursor manipulation (used by panels on Tab/click)
    void setCursorToEnd();
    void setCursorPos(int pos);
    int getCursorPos() const { return cursorPos_; }
    void setSelectionAnchor(int pos) { selectionAnchor_ = clamp(pos); }
    int getSelectionAnchor() const { return selectionAnchor_; }
    void clearSelection() { selectionAnchor_ = cursorPos_; }

private:
    std::string text_;
    int cursorPos_ = 0;
    int selectionAnchor_ = 0;
    bool active_ = false;
    bool mouseDown_ = false;
    int maxLen_ = 0; // 0 = no limit

    bool deleteSelection();
    void moveCursor(int newPos, bool extendSelection);
    int clamp(int pos) const;
};
