#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <functional>
#include <string>

class PaneContainer : public Widget {
public:
  PaneContainer(int x, int y, int w, int h, const std::string &initialType,
                FontManager &fontMgr);

  void setRotation(const std::vector<std::string> &types, int intervalS,
                   bool syncRotation = false);
  void setPaused(bool paused);
  bool isPaused() const;
  void forceAdvance();
  void jumpToType(const std::string &typeId);
  const std::vector<std::string> &getRotation() const { return rotation_; }
  const std::string &getActiveType() const { return currentType_; }

  void setWidgetFactory(std::function<Widget *(const std::string &)> factory) {
    widgetFactory_ = factory;
  }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool onMouseWheel(int scrollY) override;

  // Semantic Debug API
  std::string getName() const override {
    return "PaneContainer_" + std::to_string(paneIndex_);
  }
  std::string getDisplayName() const override;
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

  void setTheme(const std::string &theme) override {
    Widget::setTheme(theme);
    if (activeWidget_)
      activeWidget_->setTheme(theme);
  }

  bool isModalActive() const override {
    return activeWidget_ && activeWidget_->isModalActive();
  }

  void renderModal(SDL_Renderer *renderer) override {
    if (activeWidget_)
      activeWidget_->renderModal(renderer);
  }

  // Callback signature: void(int paneIndex, int mx, int my)
  void setOnSelectionRequested(std::function<void(int, int, int)> cb,
                               int paneIndex) {
    onSelectionRequested_ = cb;
    paneIndex_ = paneIndex;
  }

  // Callback signature: void(const std::string &typeId)
  void setOnConfigRequested(std::function<void(const std::string &)> cb) {
    onConfigRequested_ = cb;
  }

  void setExpanded(bool expanded) { expanded_ = expanded; }
  bool isExpanded() const { return expanded_; }
  void setOnMaximizeRequested(std::function<void(int)> cb) {
    onMaximizeRequested_ = cb;
  }
  void setLineAATexture(SDL_Texture *tex) { lineAATex_ = tex; }

private:
  std::string currentType_;
  Widget *activeWidget_ = nullptr;
  FontManager &fontMgr_;
  std::vector<std::string> rotation_;
  size_t rotationIdx_ = 0;
  Uint32 lastRotateMs_ = 0;
  int intervalS_ = 30;
  bool syncRotation_ = false;
  bool paused_ = false;
  std::function<Widget *(const std::string &)> widgetFactory_;

  int paneIndex_ = 0;
  std::function<void(int, int, int)> onSelectionRequested_;
  std::function<void(const std::string &)> onConfigRequested_;

  bool expanded_ = false;
  SDL_Rect maxBtnRect_ = {0, 0, 0, 0};
  std::function<void(int)> onMaximizeRequested_;
  SDL_Texture *lineAATex_ = nullptr;

  void activateRotationIndex(size_t idx);
};
