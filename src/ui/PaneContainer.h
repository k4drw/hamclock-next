#pragma once

#include "../core/WidgetType.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <functional>

class PaneContainer : public Widget {
public:
  PaneContainer(int x, int y, int w, int h, WidgetType initialType,
                FontManager &fontMgr);

  void setRotation(const std::vector<WidgetType> &types, int intervalS);
  const std::vector<WidgetType> &getRotation() const { return rotation_; }
  WidgetType getActiveType() const { return currentType_; }

  void setWidgetFactory(std::function<Widget *(WidgetType)> factory) {
    widgetFactory_ = factory;
  }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Semantic Debug API
  std::string getName() const override {
    return "PaneContainer_" + std::to_string(paneIndex_);
  }
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

  // Callback signature: void(WidgetType type)
  void setOnConfigRequested(std::function<void(WidgetType)> cb) {
    onConfigRequested_ = cb;
  }

private:
  WidgetType currentType_;
  Widget *activeWidget_ = nullptr;
  FontManager &fontMgr_;
  std::vector<WidgetType> rotation_;
  size_t rotationIdx_ = 0;
  Uint32 lastRotateMs_ = 0;
  int intervalS_ = 30;
  std::function<Widget *(WidgetType)> widgetFactory_;

  int paneIndex_ = 0;
  std::function<void(int, int, int)> onSelectionRequested_;
  std::function<void(WidgetType)> onConfigRequested_;
};
