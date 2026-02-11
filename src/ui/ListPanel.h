#pragma once

#include "FontManager.h"
#include "Widget.h"

#include <string>
#include <vector>

class ListPanel : public Widget {
public:
    ListPanel(int x, int y, int w, int h,
              FontManager& fontMgr,
              const std::string& title,
              const std::vector<std::string>& rows);
    ~ListPanel() override { destroyCache(); }

    void update() override {}
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

    void setRows(const std::vector<std::string>& rows);

private:
    void destroyCache();

    FontManager& fontMgr_;
    std::string title_;
    std::vector<std::string> rows_;

    SDL_Texture* titleTex_ = nullptr;
    int titleW_ = 0, titleH_ = 0;

    struct RowCache {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
        std::string text;
    };
    std::vector<RowCache> rowCache_;

    int titleFontSize_ = 12;
    int rowFontSize_ = 10;
    int lastTitleFontSize_ = 0;
    int lastRowFontSize_ = 0;
};
