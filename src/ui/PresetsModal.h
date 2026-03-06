#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "TextInput.h"

#include <SDL.h>
#include <functional>
#include <vector>

// Modal dialog for saving and recalling configuration presets.
// Centered on the 800×480 logical viewport; rendered via TimePanel::renderModal().
class PresetsModal {
public:
  explicit PresetsModal(FontManager &fontMgr);

  void init(AppConfig *cfg, std::function<void()> onApply);
  void open();
  bool isActive() const { return active_; }

  void render(SDL_Renderer *renderer);
  bool onMouseDown(int mx, int my, Uint16 mod);
  bool onMouseUp(int mx, int my, Uint16 mod);
  bool onKeyDown(SDL_Keycode key, Uint16 mod);
  bool onTextInput(const char *text);

private:
  void computeLayout();
  void beginSave();
  void commitSave();
  void cancelSave();
  void applyPreset(int index);
  void deletePreset(int index);

  static constexpr int kModalW    = 420;
  static constexpr int kModalH    = 300;
  static constexpr int kRowH      = 30;
  static constexpr int kVisibleRows = 5;
  static constexpr int kLogicalW  = 800;
  static constexpr int kLogicalH  = 480;

  FontManager &fontMgr_;
  AppConfig   *cfg_     = nullptr;
  std::function<void()> onApply_;

  bool active_       = false;
  bool saving_       = false;
  int  scrollOffset_ = 0;

  TextInput nameInput_;

  // Layout rects (absolute logical coords)
  SDL_Rect dialogRect_   = {};
  SDL_Rect closeBtnRect_ = {};
  SDL_Rect scrollUpRect_ = {};
  SDL_Rect scrollDnRect_ = {};
  SDL_Rect saveAreaRect_ = {};  // row where Save button or name input lives
  SDL_Rect saveBtnRect_  = {};
  SDL_Rect nameFieldRect_= {};
  SDL_Rect nameOkRect_   = {};
  SDL_Rect listRect_     = {};
  SDL_Rect doneBtnRect_  = {};

  // Populated each render call — bounding rects for each visible row's buttons
  struct RowRects { SDL_Rect apply; SDL_Rect del; };
  std::vector<RowRects> rowRects_;
};
