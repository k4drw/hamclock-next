#pragma once

#include "FontManager.h"
#include "Widget.h"
#include "../core/SolarData.h"

#include <memory>
#include <string>

class SpaceWeatherPanel : public Widget {
public:
    SpaceWeatherPanel(int x, int y, int w, int h,
                      FontManager& fontMgr,
                      std::shared_ptr<SolarDataStore> store);
    ~SpaceWeatherPanel() override { destroyCache(); }

    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache();
    static SDL_Color colorForK(int k);
    static SDL_Color colorForSFI(int sfi);

    FontManager& fontMgr_;
    std::shared_ptr<SolarDataStore> store_;

    static constexpr int kNumItems = 4;
    struct Item {
        std::string label;
        std::string value;
        std::string lastValue;
        SDL_Color valueColor = {255, 255, 255, 255};
        SDL_Color lastValueColor = {0, 0, 0, 0};
        SDL_Texture* labelTex = nullptr;
        SDL_Texture* valueTex = nullptr;
        int labelW = 0, labelH = 0;
        int valueW = 0, valueH = 0;
    };
    Item items_[kNumItems];

    int labelFontSize_ = 10;
    int valueFontSize_ = 24;
    int lastLabelFontSize_ = 0;
    int lastValueFontSize_ = 0;
    bool dataValid_ = false;
};
