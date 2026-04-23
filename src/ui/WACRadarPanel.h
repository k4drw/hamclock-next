#pragma once

#include "../core/ADIFData.h"
#include "Widget.h"
#include <memory>
#include <vector>

class WACRadarPanel : public Widget {
public:
    WACRadarPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store);

    void update() override;
    void render(SDL_Renderer *renderer) override;
    void onResize(int x, int y, int w, int h) override;

    std::string getName() const override { return "WACRadarPanel"; }
    const char *typeId() const override { return "wac_radar"; }
    std::string getDisplayName() const override { return "WAC Radar"; }

private:
    FontManager &fontMgr_;
    std::shared_ptr<ADIFStore> store_;
    ADIFStats lastStats_;
    int lastTotalQSOs_ = -1;

    struct ContinentInfo {
        std::string code; // NA, SA, EU, AF, AS, OC
        std::string label;
        float startAngle;
        float endAngle;
    };
    std::vector<ContinentInfo> continents_;
};
