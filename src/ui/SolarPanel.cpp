#include "SolarPanel.h"

#include <algorithm>
#include <cstdio>

void SolarPanel::update() {
  SolarData data = store_->get();
  if (!data.valid) {
    currentText_ = "Solar: awaiting data...";
  } else {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "SFI:%d  K:%d  A:%d  SSN:%d", data.sfi,
                  data.k_index, data.a_index, data.sunspot_number);
    currentText_ = buf;
  }
}

void SolarPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  bool needRedraw = (currentText_ != lastText_) || (fontSize_ != lastFontSize_);
  if (needRedraw) {
    destroyCache();
    SDL_Color green = {0, 255, 128, 255};
    cached_ = fontMgr_.renderText(renderer, currentText_, green, fontSize_,
                                  &texW_, &texH_);
    lastText_ = currentText_;
    lastFontSize_ = fontSize_;
  }

  if (cached_) {
    // Left-aligned with 2% padding, vertically centered
    int drawX = x_ + static_cast<int>(width_ * 0.02f);
    int drawY = y_ + (height_ - texH_) / 2;
    SDL_Rect dst = {drawX, drawY, texW_, texH_};
    SDL_RenderCopy(renderer, cached_, nullptr, &dst);
  }
}

void SolarPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Cap to prevent giant text in taller top bar; width-aware for long data
  // string
  fontSize_ = std::clamp(static_cast<int>(w * 0.05f), 8, 22);
  destroyCache();
}
