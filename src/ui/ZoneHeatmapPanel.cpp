#include "ZoneHeatmapPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <algorithm>
#include <fmt/core.h>

ZoneHeatmapPanel::ZoneHeatmapPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void ZoneHeatmapPanel::update() {
    ADIFStats stats = store_->get();
    if (!stats.valid) return;
    if (stats.totalQSOs != lastTotalQSOs_) {
        lastTotalQSOs_ = stats.totalQSOs;
        lastStats_ = stats;
    }
}

void ZoneHeatmapPanel::onResize(int x, int y, int w, int h) {
    Widget::onResize(x, y, w, h);
}

bool ZoneHeatmapPanel::onMouseUp(int mx, int my, int button) {
    if (mx > x_ && mx < x_ + width_ && my > y_ && my < y_ + 25) {
        modeITU_ = !modeITU_;
        return true;
    }
    return false;
}

void ZoneHeatmapPanel::render(SDL_Renderer *renderer) {
    if (!fontMgr_.ready()) return;

    ThemeColors themes = getThemeColors(theme_);
    renderChrome(renderer);
    
    std::string title = modeITU_ ? "ITU Zones" : "CQ Zones";
    renderTitle(renderer, fontMgr_, title);

    auto *cat = fontMgr_.catalog();
    if (!cat) return;

    int maxZone = modeITU_ ? 75 : 40;
    int cols = modeITU_ ? 15 : 10;
    int rows = (maxZone + cols - 1) / cols;

    float pad = 4.0f;
    float availW = width_ - 2.0f * pad;
    float availH = height_ - 30.0f - pad;
    float cellW = availW / cols;
    float cellH = availH / rows;

    const auto& worked = modeITU_ ? lastStats_.workedZonesITU : lastStats_.workedZonesCQ;
    const auto& confirmed = modeITU_ ? lastStats_.confirmedZonesITU : lastStats_.confirmedZonesCQ;

    for (int i = 0; i < maxZone; ++i) {
        int zone = i + 1;
        int r = i / cols;
        int c = i % cols;

        float vx = x_ + pad + c * cellW;
        float vy = y_ + 25.0f + r * cellH;

        bool isWorked = worked.count(zone) > 0;
        bool isConfirmed = confirmed.count(zone) > 0;

        SDL_Color cellColor = themes.rowStripe1;
        if (isConfirmed) cellColor = themes.success;
        else if (isWorked) cellColor = themes.warning;

        SDL_Rect rect = {(int)vx + 1, (int)vy + 1, (int)cellW - 1, (int)cellH - 1};
        SDL_SetRenderDrawColor(renderer, cellColor.r, cellColor.g, cellColor.b, cellColor.a);
        SDL_RenderFillRect(renderer, &rect);

        if (cellW > 12) {
            SDL_Color txtCol = (isWorked || isConfirmed) ? themes.bg : themes.textDim;
            cat->drawText(renderer, std::to_string(zone), vx + cellW/2, vy + cellH/2, txtCol, FontStyle::Tiny, true, false, true);
        }
    }
    
    // Summary
    int totalWorked = worked.size();
    int totalConfirmed = confirmed.size();
    std::string summary = fmt::format("W: {}/{} C: {}/{}", totalWorked, maxZone, totalConfirmed, maxZone);
    cat->drawText(renderer, summary, x_ + width_ / 2, y_ + height_ - 10, themes.textDim, FontStyle::Tiny, true, false, true);
}

REGISTER_WIDGET("zone_heatmap", "Zone Heatmap", false, false, {
    return std::make_unique<ZoneHeatmapPanel>(0, 0, 0, 0, deps.fontMgr, deps.adifStore);
})
