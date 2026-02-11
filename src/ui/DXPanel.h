#pragma once

#include "FontManager.h"
#include "Widget.h"
#include "../core/HamClockState.h"

#include <memory>
#include <string>

class DXPanel : public Widget {
public:
    DXPanel(int x, int y, int w, int h,
            FontManager& fontMgr,
            std::shared_ptr<HamClockState> state);
    ~DXPanel() override { destroyCache(); }

    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache();

    FontManager& fontMgr_;
    std::shared_ptr<HamClockState> state_;

    // Up to 6 lines: "DX:", grid, coords, bearing, distance, (or "Select target")
    static constexpr int kNumLines = 6;
    SDL_Texture* lineTex_[kNumLines] = {};
    int lineW_[kNumLines] = {};
    int lineH_[kNumLines] = {};
    std::string lineText_[kNumLines];
    std::string lastLineText_[kNumLines];

    int lineFontSize_[kNumLines] = {};
    int lastLineFontSize_[kNumLines] = {};
};
