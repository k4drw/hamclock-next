#include "NetLoggerPanel.h"
#include "WidgetRegistry.h"
#include "../services/NetLoggerProvider.h"
#include "MapContext.h"
#include <algorithm>

REGISTER_WIDGET("netlogger", "NetLogger", true, false, {
    return std::make_unique<NetLoggerPanel>(0, 0, 0, 0, deps.fontMgr, deps.netLoggerStore);
})

NetLoggerPanel::NetLoggerPanel(int x, int y, int w, int h, FontManager& fontMgr, std::shared_ptr<NetLoggerStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {
}

void NetLoggerPanel::update() {
    if (store_) {
        data_ = store_->get();
    }
    
    // Auto-scroll logic if needed, or adjust maxScroll
    if (data_ && data_->hasCheckins) {
        maxScroll_ = std::max(0, (int)data_->checkins.size() * rowHeight_ - (height_ - 25));
    } else if (data_) {
        maxScroll_ = std::max(0, (int)data_->activeNets.size() * rowHeight_ - (height_ - 25));
    }
    
    if (scrollY_ > maxScroll_) scrollY_ = maxScroll_;
    if (scrollY_ < 0) scrollY_ = 0;
}

void NetLoggerPanel::render(SDL_Renderer* renderer) {
    renderChrome(renderer);
    renderTitle(renderer, fontMgr_, "NetLogger");
    
    ThemeColors themes = getThemeColors(theme_);
    SDL_Rect contentArea = {x_ + 5, y_ + 25, width_ - 10, height_ - 30};
    
    // Set clipping rect so scrolling doesn't overflow
    SDL_RenderSetClipRect(renderer, &contentArea);
    
    if (data_) {
        if (data_->hasCheckins && !data_->selectedNetName.empty()) {
            renderCheckinList(renderer, themes);
        } else {
            renderNetList(renderer, themes);
        }
    }
    
    SDL_RenderSetClipRect(renderer, nullptr);
}

void NetLoggerPanel::renderNetList(SDL_Renderer* renderer, const ThemeColors& themes) {
    auto* cat = fontMgr_.catalog();
    int cy = y_ + 25 - scrollY_;
    
    if (!data_ || data_->activeNets.empty()) {
        cat->drawText(renderer, "Loading nets...", x_ + 10, cy, themes.textDim, FontStyle::SmallRegular);
        return;
    }
    
    for (size_t i = 0; i < data_->activeNets.size(); ++i) {
        const auto& net = data_->activeNets[i];
        
        // Skip rendering if completely out of bounds
        if (cy + rowHeight_ > y_ + 25 && cy < y_ + height_ - 5) {
            // Draw row background alternating
            if (i % 2 == 1) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, themes.rowStripe1.a);
                SDL_Rect rowRect = {x_ + 5, cy, width_ - 10, rowHeight_};
                SDL_RenderFillRect(renderer, &rowRect);
            }
            
            // Name (truncated based on width)
            std::string nameStr = net.netName;
            int ptSize = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
            int maxNameWidth = width_ - 20;
            while (nameStr.length() > 3 && fontMgr_.getLogicalWidth(nameStr, ptSize) > maxNameWidth) {
                nameStr = nameStr.substr(0, nameStr.length() - 4) + "...";
            }
            cat->drawText(renderer, nameStr, x_ + 10, cy + 3, themes.text, FontStyle::SmallRegular);
            
            // Freq on second line
            cat->drawText(renderer, net.frequency, x_ + 10, cy + 20, themes.accent, FontStyle::SmallRegular);
            
            // Users on second line, right aligned
            std::string users = std::to_string(net.subscriberCount) + " chk";
            int usersWidth = fontMgr_.getLogicalWidth(users, ptSize);
            cat->drawText(renderer, users, x_ + width_ - 10 - usersWidth, cy + 20, themes.textDim, FontStyle::SmallRegular);
        }
        cy += rowHeight_;
    }
}

void NetLoggerPanel::renderCheckinList(SDL_Renderer* renderer, const ThemeColors& themes) {
    auto* cat = fontMgr_.catalog();
    int cy = y_ + 25 - scrollY_;
    
    // Header for returning to nets
    SDL_Rect headerRect = {x_ + 5, cy, width_ - 10, 25};
    if (cy + 25 > y_ + 25 && cy < y_ + height_ - 5) {
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 50);
        SDL_RenderFillRect(renderer, &headerRect);
        
        bool isHovered = (my_ >= cy && my_ < cy + 25 && mx_ >= x_ + 5 && mx_ < x_ + width_ - 5);
        SDL_Color color = isHovered ? themes.accent : themes.text;
        cat->drawText(renderer, "< Back to Nets", x_ + 10, cy + 5, color, FontStyle::SmallRegular);
    }
    cy += 25;
    
    if (!data_) return;
    
    for (size_t i = 0; i < data_->checkins.size(); ++i) {
        const auto& c = data_->checkins[i];
        
        if (cy + rowHeight_ > y_ + 25 && cy < y_ + height_ - 5) {
            if (i % 2 == 1) {
                SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, themes.rowStripe1.a);
                SDL_Rect rowRect = {x_ + 5, cy, width_ - 10, rowHeight_};
                SDL_RenderFillRect(renderer, &rowRect);
            }
            
            int ptSize = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
            
            // Callsign
            cat->drawText(renderer, c.callsign, x_ + 10, cy + 3, themes.text, FontStyle::SmallRegular);
            
            // First Name (truncated to fit remaining space)
            int callWidth = fontMgr_.getLogicalWidth(c.callsign, ptSize);
            std::string fn = c.firstName;
            int maxFnWidth = width_ - 20 - callWidth - 10;
            while (fn.length() > 3 && fontMgr_.getLogicalWidth(fn, ptSize) > maxFnWidth) {
                fn = fn.substr(0, fn.length() - 4) + "...";
            }
            cat->drawText(renderer, fn, x_ + 20 + callWidth, cy + 3, themes.textDim, FontStyle::SmallRegular);
            
            // State & Grid on second line
            std::string loc = c.state;
            if (!c.grid.empty()) loc += (loc.empty() ? "" : " ") + c.grid;
            cat->drawText(renderer, loc, x_ + 10, cy + 20, themes.textDim, FontStyle::SmallRegular);
        }
        cy += rowHeight_;
    }
}

bool NetLoggerPanel::onMouseWheel(int scrollY) {
    scrollY_ -= scrollY * rowHeight_ * 2;
    if (scrollY_ > maxScroll_) scrollY_ = maxScroll_;
    if (scrollY_ < 0) scrollY_ = 0;
    return true;
}

void NetLoggerPanel::onMouseMove(int mx, int my) {
    mx_ = mx;
    my_ = my;
}

bool NetLoggerPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
    if (my < y_ + 25 || my > y_ + height_ - 5) return false;
    
    int relY = my - (y_ + 25) + scrollY_;
    
    if (data_ && data_->hasCheckins && !data_->selectedNetName.empty()) {
        if (relY < 25) {
            // Clicked < Back to Nets
            if (store_) {
                store_->setSelectedNet("", "");
            }
            scrollY_ = 0;
            return true;
        }
    } else if (data_) {
        int clickedRow = relY / rowHeight_;
        if (clickedRow >= 0 && clickedRow < (int)data_->activeNets.size()) {
            const auto& net = data_->activeNets[clickedRow];
            if (store_) {
                store_->setSelectedNet(net.serverName, net.netName);
            }
            scrollY_ = 0;
            return true;
        }
    }
    
    return true;
}
