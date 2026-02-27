#pragma once

#include "ColorPicker.h"
#include "FontManager.h"
#include "Widget.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace HamClock {

class ThemeCustomizer : public Widget {
public:
  ThemeCustomizer(int x, int y, int w, int h, FontManager &fontMgr,
                  std::string &theme,
                  std::map<std::string, SDL_Color> &overrides);
  ~ThemeCustomizer() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseDown(int mx, int my, Uint16 mod) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  void onMouseMove(int mx, int my) override;

  bool isActive() const { return active_; }
  void setActive(bool active);

private:
  bool active_ = false;
  std::unique_ptr<ColorPicker> colorPicker_;

  struct ColorKey {
    std::string key;
    std::string label;
  };
  std::vector<ColorKey> colorKeys_;
  int selectedIndex_ = 0;
  FontManager &fontMgr_;
  std::string &theme_;
  std::map<std::string, SDL_Color> &overrides_;

  SDL_Rect rectList_;
  SDL_Rect rectOk_;
  SDL_Rect rectCancel_;

  std::map<std::string, SDL_Color> overridesBackup_;
  std::string themeBackup_;

  void calculateLayout();
  void loadOverrides();
  void saveOverrides();
};

} // namespace HamClock
