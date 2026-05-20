#include "WACRadarPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cmath>

WACRadarPanel::WACRadarPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {
    continents_ = {
        {"NA", "NA", -90.0f, -30.0f},
        {"SA", "SA", -30.0f, 30.0f},
        {"AF", "AF", 30.0f, 90.0f},
        {"EU", "EU", 90.0f, 150.0f},
        {"AS", "AS", 150.0f, 210.0f},
        {"OC", "OC", 210.0f, 270.0f}
    };
}

void WACRadarPanel::update() {
    ADIFStats stats = store_->get();
    if (!stats.valid) return;
    if (stats.totalQSOs != lastTotalQSOs_) {
        lastTotalQSOs_ = stats.totalQSOs;
        lastStats_ = stats;
    }
}

void WACRadarPanel::onResize(int x, int y, int w, int h) {
    Widget::onResize(x, y, w, h);
}

void WACRadarPanel::render(SDL_Renderer *renderer) {
    if (!fontMgr_.ready()) return;

    ThemeColors themes = getThemeColors(theme_);
    renderChrome(renderer);
    renderTitle(renderer, fontMgr_, "WAC Radar");

    auto *cat = fontMgr_.catalog();
    if (!cat) return;

    float cx = x_ + width_ / 2.0f;
    float cy = y_ + (height_ + 15) / 2.0f;
    float radius = std::min(width_, height_ - 25) * 0.38f;

    for (const auto &ci : continents_) {
        bool worked = lastStats_.workedContinents.count(ci.code) > 0;
        bool confirmed = lastStats_.confirmedContinents.count(ci.code) > 0;

        SDL_Color segColor = themes.rowStripe1;
        if (confirmed) segColor = themes.success;
        else if (worked) segColor = themes.warning;

        RenderUtils::drawPie(renderer, cx, cy, radius, ci.startAngle, ci.endAngle, segColor);
        RenderUtils::drawArcOutline(renderer, cx, cy, radius, ci.startAngle, ci.endAngle, 1.5f, themes.border);

        // Label position
        float midAngle = (ci.startAngle + ci.endAngle) / 2.0f * 3.14159265f / 180.0f;
        float lx = cx + (radius * 0.7f) * std::cos(midAngle);
        float ly = cy + (radius * 0.7f) * std::sin(midAngle);

        SDL_Color labelCol = (worked || confirmed) ? themes.bg : themes.textDim;
        cat->drawText(renderer, ci.label, lx, ly, labelCol, FontStyle::Tiny, true, false, true);
    }

    // Legend
    int lx = x_ + 6;
    int ly = y_ + height_ - 14;
    cat->drawText(renderer, "W", lx, ly, themes.warning, FontStyle::Tiny);
    cat->drawText(renderer, "Worked", lx + 12, ly, themes.textDim, FontStyle::Tiny);
    cat->drawText(renderer, "C", lx + 55, ly, themes.success, FontStyle::Tiny);
    cat->drawText(renderer, "Confirmed", lx + 65, ly, themes.textDim, FontStyle::Tiny);
}

REGISTER_WIDGET("wac_radar", "WAC Radar", false, false, {
    return std::make_unique<WACRadarPanel>(0, 0, 0, 0, deps.fontMgr, deps.adifStore);
})
