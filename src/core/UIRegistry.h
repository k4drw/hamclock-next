#pragma once

#include <SDL2/SDL.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct SemanticAction {
  std::string name;
  SDL_Rect rect;
};

struct WidgetInfo {
  std::string name;
  SDL_Rect rect;
  std::vector<SemanticAction> actions;
};

class UIRegistry {
public:
  static UIRegistry &getInstance() {
    static UIRegistry instance;
    return instance;
  }

  void updateWidget(const std::string &id, const WidgetInfo &info) {
    std::lock_guard<std::mutex> lock(mutex_);
    widgets_[id] = info;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    widgets_.clear();
  }

  std::map<std::string, WidgetInfo> getSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return widgets_;
  }

  void setScale(float scale, int offX, int offY) {
    std::lock_guard<std::mutex> lock(mutex_);
    scale_ = scale;
    offX_ = offX;
    offY_ = offY;
  }

  void getScale(float &scale, int &offX, int &offY) {
    std::lock_guard<std::mutex> lock(mutex_);
    scale = scale_;
    offX = offX_;
    offY = offY_;
  }

private:
  UIRegistry() = default;
  std::mutex mutex_;
  std::map<std::string, WidgetInfo> widgets_;
  float scale_ = 1.0f;
  int offX_ = 0;
  int offY_ = 0;
};
