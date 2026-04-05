#pragma once

#include "ColorPicker.h"
#include "FontManager.h"
#include "Widget.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace HamClock {

/**
 * @brief Widget for customizing the 5-point propagation gradient.
 */
class PropColorCustomizer : public Widget {
public:
  PropColorCustomizer(int x, int y, int w, int h, FontManager &fontMgr,
                      const std::string &theme,
                      std::map<std::string, SDL_Color> &overrides,
                      std::function<void()> onUpdate);
  ~PropColorCustomizer() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
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
  std::map<std::string, SDL_Color> &overrides_;
  std::function<void()> onUpdate_;

  SDL_Rect rectList_;
  SDL_Rect rectOk_;
  SDL_Rect rectCancel_;

  std::map<std::string, SDL_Color> overridesBackup_;

  void calculateLayout();
  void loadOverrides();
};

} // namespace HamClock
