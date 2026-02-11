#pragma once

#include "FontManager.h"
#include "Widget.h"
#include "../core/OrbitPredictor.h"

#include <string>
#include <vector>

class SatPanel : public Widget {
public:
    SatPanel(int x, int y, int w, int h, FontManager& fontMgr);
    ~SatPanel() override { destroyCache(); }

    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

    // Set the predictor to display (non-owning). Pass nullptr to clear.
    void setPredictor(OrbitPredictor* pred) { predictor_ = pred; }

    // True if a predictor is set and ready.
    bool hasPredictor() const { return predictor_ && predictor_->isReady(); }

private:
    void destroyCache();
    void renderPolarPlot(SDL_Renderer* renderer, int cx, int cy, int radius);

    FontManager& fontMgr_;
    OrbitPredictor* predictor_ = nullptr;

    // Text lines: name, rise/set info, az/el, TLE age
    static constexpr int kNumLines = 4;
    SDL_Texture* lineTex_[kNumLines] = {};
    int lineW_[kNumLines] = {};
    int lineH_[kNumLines] = {};
    std::string lineText_[kNumLines];
    std::string lastLineText_[kNumLines];
    int lineFontSize_[kNumLines] = {};
    int lastLineFontSize_[kNumLines] = {};

    // Compass labels for polar plot (N, NE, E, SE, S, SW, W, NW)
    static constexpr int kNumCompass = 8;
    SDL_Texture* compassTex_[kNumCompass] = {};
    int compassW_[kNumCompass] = {};
    int compassH_[kNumCompass] = {};
    int compassFontSize_ = 0;
    int lastCompassFontSize_ = 0;

    // Pass trajectory for polar plot (az/el pairs)
    struct AzElPoint { double az; double el; };
    std::vector<AzElPoint> passTrack_;
    AzElPoint currentPos_ = {0, 0};
    bool satAboveHorizon_ = false;

    // Font sizes
    int nameFontSize_ = 14;
    int infoFontSize_ = 10;

    // Throttle updates to avoid recalculating every frame
    std::time_t lastUpdate_ = 0;
};
