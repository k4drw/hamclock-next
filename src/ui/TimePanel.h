#pragma once

#include "FontManager.h"
#include "Widget.h"

#include <array>
#include <functional>
#include <string>

class TextureManager;

class TimePanel : public Widget {
public:
  TimePanel(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, const std::string &callsign);
  ~TimePanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Semantic Debug API
  std::string getName() const override { return "TimePanel"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;

  bool isEditing() const { return editing_; }

  void setCallColor(SDL_Color color) {
    callColor_ = color;
    // Find matching palette index, default to 0 if no match
    selectedColorIdx_ = 0;
    for (int i = 0; i < kNumColors; ++i) {
      if (kPalette[i].r == color.r && kPalette[i].g == color.g &&
          kPalette[i].b == color.b) {
        selectedColorIdx_ = i;
        break;
      }
    }
    if (callTex_) {
      SDL_DestroyTexture(callTex_);
      callTex_ = nullptr;
    }
  }

  bool isSetupRequested() const { return setupRequested_; }
  void clearSetupRequest() { setupRequested_ = false; }

  // Callback invoked when callsign text or color is changed via the editor.
  using ConfigChangedCb =
      std::function<void(const std::string &callsign, SDL_Color color)>;
  void setOnConfigChanged(ConfigChangedCb cb) {
    onConfigChanged_ = std::move(cb);
  }

private:
  void destroyCache();
  void renderEditOverlay(SDL_Renderer *renderer);
  void startEditing();
  void stopEditing(bool apply);

  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::string callsign_;
  SDL_Color callColor_ = {255, 165, 0, 255}; // default orange

  // Editor state
  bool editing_ = false;
  std::string editText_;
  int cursorPos_ = 0;
  int selectedColorIdx_ = 2; // default to orange

  static constexpr int kNumColors = 12;
  static constexpr std::array<SDL_Color, kNumColors> kPalette = {{
      {255, 255, 255, 255}, // White
      {255, 50, 50, 255},   // Red
      {255, 165, 0, 255},   // Orange
      {255, 255, 0, 255},   // Yellow
      {0, 255, 0, 255},     // Green
      {0, 200, 255, 255},   // Cyan
      {0, 100, 255, 255},   // Blue
      {160, 32, 240, 255},  // Purple
      {255, 105, 180, 255}, // Pink
      {255, 0, 255, 255},   // Magenta
      {128, 255, 0, 255},   // Lime
      {255, 215, 0, 255},   // Gold
  }};

  // Cached textures
  SDL_Texture *callTex_ = nullptr;
  int callW_ = 0, callH_ = 0;

  SDL_Texture *hmTex_ = nullptr; // HH:MM
  int hmW_ = 0, hmH_ = 0;
  std::string lastHM_;

  SDL_Texture *secTex_ = nullptr; // :SS
  int secW_ = 0, secH_ = 0;
  std::string lastSec_;

  SDL_Texture *dateTex_ = nullptr;
  int dateW_ = 0, dateH_ = 0;
  std::string lastDate_;

  std::string currentHM_;
  std::string currentSec_;
  std::string currentDate_;

  int callFontSize_ = 20;
  int hmFontSize_ = 60;
  int secFontSize_ = 30;
  int dateFontSize_ = 14;
  int lastCallFontSize_ = 0;
  int lastHmFontSize_ = 0;
  int lastSecFontSize_ = 0;
  int lastDateFontSize_ = 0;

  ConfigChangedCb onConfigChanged_;
  bool setupRequested_ = false;
  SDL_Rect gearRect_ = {};
  int gearSize_ = 12;

  // Info bar state (uptime, rotating center, version)
  std::string currentUptime_;
  std::string infoTexts_[3]; // CPU temp, Disk %, Local IP
  int infoRotateIdx_ = 0;
  Uint32 lastInfoRotateMs_ = 0;
  int infoFontSize_ = 10;
};
