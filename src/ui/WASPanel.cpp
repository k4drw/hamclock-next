#include "WASPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"
#include <algorithm>

static const char *kStates[] = {
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY"
};

WASPanel::WASPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {
    initStates();
}

void WASPanel::initStates() {
    states_.clear();
    for (const char *abbr : kStates) {
        states_.push_back({abbr, false, false});
    }
}

void WASPanel::update() {
    stats_ = store_->get();
    updateStates();
}

void WASPanel::updateStates() {
    for (auto &s : states_) {
        s.worked = stats_.workedStates.count(s.abbr) > 0;
        s.confirmed = stats_.confirmedStates.count(s.abbr) > 0;
    }
}

void WASPanel::render(SDL_Renderer *renderer) {
    if (!fontMgr_.ready()) return;

    renderChrome(renderer);
    ThemeColors themes = getThemeColors(theme_);
    auto *cat = fontMgr_.catalog();

    int pad = 4;
    int curY = y_ + 5;

    cat->drawText(renderer, "WAS Progress", x_ + pad, curY, themes.accent, FontStyle::MicroBold);
    
    char buf[32];
    std::snprintf(buf, sizeof(buf), "W:%zu C:%zu", stats_.workedStates.size(), stats_.confirmedStates.size());
    cat->drawText(renderer, buf, x_ + width_ - pad, curY, themes.textDim, FontStyle::Tiny, false, true);
    
    curY += 16;

    // 10x5 grid
    int rows = 5;
    int cols = 10;
    float cellW = (width_ - 2 * pad) / (float)cols;
    float cellH = (height_ - (curY - y_) - pad) / (float)rows;

    for (int i = 0; i < 50; ++i) {
        int r = i / cols;
        int c = i % cols;
        
        SDL_Rect rect = {
            (int)(x_ + pad + c * cellW),
            (int)(curY + r * cellH),
            (int)cellW - 1,
            (int)cellH - 1
        };

        SDL_Color fill = themes.rowStripe1; // Gray / Default
        if (states_[i].confirmed) {
            fill = themes.success; // Cyan/Green
        } else if (states_[i].worked) {
            fill = themes.accent; // Yellow/Accent
        }

        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
        SDL_RenderFillRect(renderer, &rect);
        
        // State label
        SDL_Color textCol = states_[i].worked ? themes.bg : themes.textDim;
        cat->drawText(renderer, states_[i].abbr, rect.x + rect.w/2, rect.y + rect.h/2, 
                     textCol, FontStyle::Fast, true, false, true);
    }
}

REGISTER_WIDGET("was_progress", "WAS Progress", false, false, {
    return std::make_unique<WASPanel>(0, 0, 0, 0, deps.fontMgr, deps.adifStore);
})
