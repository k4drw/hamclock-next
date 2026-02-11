#pragma once

#include "FontManager.h"
#include "Widget.h"

#include <string>

class PlaceholderWidget : public Widget {
public:
    PlaceholderWidget(int x, int y, int w, int h,
                      FontManager& fontMgr,
                      const std::string& title, SDL_Color titleColor)
        : Widget(x, y, w, h)
        , fontMgr_(fontMgr)
        , title_(title)
        , titleColor_(titleColor) {}

    ~PlaceholderWidget() override { destroyCache(); }

    void update() override {}
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache() {
        if (cached_) { SDL_DestroyTexture(cached_); cached_ = nullptr; }
    }

    FontManager& fontMgr_;
    std::string title_;
    SDL_Color titleColor_;
    SDL_Texture* cached_ = nullptr;
    int texW_ = 0;
    int texH_ = 0;
    int fontSize_ = 14;
    int lastFontSize_ = 0;
};
