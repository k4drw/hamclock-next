#pragma once

#include "../core/ConfigManager.h"
#include "../core/MemoryMonitor.h"
#include "../core/RigData.h"
#include "FontManager.h"
#include "PresetsModal.h"
#include "TextInput.h"
#include "Widget.h"

#include <SDL.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

class TextureManager;

class TimePanel : public Widget {
public:
  TimePanel(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, AppConfig &config);
  ~TimePanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Semantic Debug API
  std::string getName() const override { return "TimePanel"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

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
    MemoryMonitor::getInstance().destroyTexture(callTex_);
  }

  // Preset modal interface — call once after timePanel is constructed.
  void initPresets(AppConfig *cfg, std::function<void()> onApply);
  bool isModalActive() const override;
  void renderModal(SDL_Renderer *renderer) override;

  bool isSetupRequested() const { return setupRequested_; }
  void clearSetupRequest() { setupRequested_ = false; }

  bool isHelpRequested() const { return helpRequested_; }
  void clearHelpRequest() { helpRequested_ = false; }

  // Rotation transport controls
  void setOnPauseRotation(std::function<void()> cb) {
    onPauseRotation_ = std::move(cb);
  }
  void setOnNextRotation(std::function<void()> cb) {
    onNextRotation_ = std::move(cb);
  }
  void setRotationPaused(bool paused) { rotationPaused_ = paused; }

  // Called whenever the update checker has new information.
  void setUpdateInfo(bool available, const std::string &latestVersion) {
    updateAvailable_ = available;
    latestVersion_ = latestVersion;
  }

  bool isUpdateRequested() const { return updateRequested_; }
  void clearUpdateRequest() { updateRequested_ = false; }

  void setCallBgColor(SDL_Color color) { callBgColor_ = color; }
  void setRigDataStore(RigDataStore *store) { rigStore_ = store; }

  // Callback invoked when callsign text or colors are changed via the editor.
  using ConfigChangedCb =
      std::function<void(const std::string &callsign, SDL_Color fgColor,
                         SDL_Color bgColor)>;
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
  AppConfig &config_;
  std::string callsign_;
  SDL_Color callColor_ = {255, 165, 0, 255}; // default orange
  SDL_Color callBgColor_ = {0, 0, 0, 0};     // default transparent (no bg)

  // Editor state
  bool editing_ = false;
  bool editingBgColor_ = false; // palette is editing bg color vs fg color
  TextInput editInput_;
  int selectedColorIdx_ = 2;  // default to orange
  int selectedBgColorIdx_ = -1; // -1 = none/transparent, 0-11 = kPalette

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
  std::string currentUptime_;
  std::array<std::string, 3> infoTexts_;
  std::mutex infoMutex_;
  uint32_t lastInfoRotateMs_ = 0;
  std::atomic<bool> infoUpdating_{false};
  int infoRotateIdx_ = 0;

  int callFontSize_ = 20;
  int hmFontSize_ = 60;
  int secFontSize_ = 30;
  int dateFontSize_ = 14;
  int lastCallFontSize_ = 0;
  int lastHmFontSize_ = 0;
  int lastSecFontSize_ = 0;
  int lastDateFontSize_ = 0;

  ConfigChangedCb onConfigChanged_;
  std::function<void()> onPauseRotation_;
  std::function<void()> onNextRotation_;
  bool rotationPaused_ = false;

  // On The Air (PTT via rigctld)
  RigDataStore *rigStore_ = nullptr;
  bool onAir_ = false;
  SDL_Rect pauseRect_ = {};
  SDL_Rect nextRect_ = {};
  bool setupRequested_ = false;
  bool updateAvailable_ = false;
  std::string latestVersion_;
  SDL_Rect gearRect_ = {};
  SDL_Rect helpRect_ = {};
  SDL_Rect versionRect_ = {};
  int gearSize_ = 12;
  bool updateRequested_ = false;
  bool helpRequested_ = false;

  // Presets
  std::unique_ptr<PresetsModal> presetsModal_;
  SDL_Rect presetsRect_ = {};
  SDL_Texture *starTex_ = nullptr;
  int starW_ = 0, starH_ = 0;

  // Info bar state (uptime, rotating center, version)
  int infoFontSize_ = 10;
};
