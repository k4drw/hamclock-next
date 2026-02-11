#include "PlaceholderWidget.h"
#include "FontCatalog.h"

void PlaceholderWidget::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Render title centered
  if (fontSize_ != lastFontSize_) {
    destroyCache();
    cached_ = fontMgr_.renderText(renderer, title_, titleColor_, fontSize_,
                                  &texW_, &texH_);
    lastFontSize_ = fontSize_;
  }

  if (cached_) {
    int drawX = x_ + (width_ - texW_) / 2;
    int drawY = y_ + (height_ - texH_) / 2;
    SDL_Rect dst = {drawX, drawY, texW_, texH_};
    SDL_RenderCopy(renderer, cached_, nullptr, &dst);
  }
}

void PlaceholderWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  fontSize_ = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
  destroyCache();
}
