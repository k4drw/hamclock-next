#pragma once

#include "../core/ADIFData.h"
#include "Widget.h"
#include <memory>

class ZoneHeatmapPanel : public Widget {
public:
    ZoneHeatmapPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store);

    void update() override;
    void render(SDL_Renderer *renderer) override;
    void onResize(int x, int y, int w, int h) override;
    bool onMouseUp(int mx, int my, int button) override;

    std::string getName() const override { return "ZoneHeatmapPanel"; }
    const char *typeId() const override { return "zone_heatmap"; }
    std::string getDisplayName() const override { return "Zone Heatmap"; }

private:
    FontManager &fontMgr_;
    std::shared_ptr<ADIFStore> store_;
    ADIFStats lastStats_;
    int lastTotalQSOs_ = -1;
    bool modeITU_ = false; // false = CQ (1-40), true = ITU (1-75)
};
