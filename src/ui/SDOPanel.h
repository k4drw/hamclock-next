#pragma once

#include "../services/SDOProvider.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <SDL.h>
#include <string>

class SDOPanel : public Widget {
public:
  SDOPanel(int x, int y, int w, int h, FontManager &fontMgr,
           TextureManager &texMgr, SDOProvider &provider);

  void setObserver(double lat, double lon) {
    obsLat_ = lat;
    obsLon_ = lon;
  }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

  // Semantic Debug API
  std::string getName() const override { return "SDOPanel"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

  bool isModalActive() const override { return menuVisible_; }
  void renderModal(SDL_Renderer *renderer) override;

private:
  void renderMenu(SDL_Renderer *renderer, const struct ThemeColors &themes);
  void renderOverlays(SDL_Renderer *renderer, const struct ThemeColors &themes);
  void recalcMenuLayout();

  FontManager &fontMgr_;
  TextureManager &texMgr_;
  SDOProvider &provider_;
  double obsLat_ = 0, obsLon_ = 0;

  struct Wavelength {
    const char *name;
    const char *id;
  };

  static inline const Wavelength wavelengths_[] = {{"Composite", "211193171"},
                                                   {"Magnetogram", "HMIB"},
                                                   {"6173A", "HMIIC"},
                                                   {"131A", "0131"},
                                                   {"193A", "0193"},
                                                   {"211A", "0211"},
                                                   {"304A", "0304"}};

  std::string currentId_ = "0193";
  bool rotating_ = false;
  bool menuVisible_ = false;
  bool imageReady_ = false;
  uint32_t lastFetch_ = 0;
  uint32_t lastRotate_ = 0;

  // Layout
  SDL_Rect menuRect_ = {0, 0, 0, 0};
  std::vector<SDL_Rect> radioRects_;
  SDL_Rect rotateRect_ = {0, 0, 0, 0};
  SDL_Rect graylineRect_ = {0, 0, 0, 0};
  SDL_Rect movieRect_ = {0, 0, 0, 0};
  SDL_Rect okRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};

  // Temp state
  std::string tempId_;
  bool tempRotating_ = false;
  bool tempGrayline_ = false;
  bool tempMovie_ = false;

  // Scaling
  int overlayFontSize_ = 14;
  int menuFontSize_ = 18;
  int itemH_ = 24;
};
