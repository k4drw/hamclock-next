#include "PowerwallPanel.h"
#include "WidgetRegistry.h"
#include "RenderUtils.h"
#include "../core/Logger.h"
#include <cmath>
#include <algorithm>

REGISTER_WIDGET("powerwall", "Powerwall", true, false, {
    return std::make_unique<PowerwallPanel>(0, 0, 0, 0, deps.fontMgr, deps.powerwallStore, deps.appCfg);
})

PowerwallPanel::PowerwallPanel(int x, int y, int w, int h, FontManager& fontMgr, std::shared_ptr<PowerwallStore> store, AppConfig& config)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)), config_(config) {}

void PowerwallPanel::update() {
    if (store_) data_ = store_->get();
    animTick_ += 30; // approx 30ms per frame
}

void PowerwallPanel::render(SDL_Renderer* renderer) {
    renderChrome(renderer);
    renderTitle(renderer, fontMgr_, "Powerwall");
    
    ThemeColors themes = getThemeColors(theme_);
    
    if (isConfiguring_) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 230);
        SDL_RenderFillRect(renderer, &modalRect_);
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
        SDL_RenderDrawRect(renderer, &modalRect_);
        
        auto* cat = fontMgr_.catalog();
        cat->drawText(renderer, "Powerwall URL:", modalRect_.x + 4, modalRect_.y + 4, themes.text, FontStyle::MicroBold);
        
        urlInput_.render(renderer, fontMgr_, urlRect_.x, urlRect_.y, urlRect_.w, urlRect_.h, FontStyle::Micro, 5, true, true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "http://...");
        
        // OK Button
        SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
        SDL_RenderFillRect(renderer, &okRect_);
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
        SDL_RenderDrawRect(renderer, &okRect_);
        int okW = fontMgr_.getLogicalWidth("OK", cat->ptSize(FontStyle::MicroBold));
        cat->drawText(renderer, "OK", okRect_.x + (okRect_.w - okW)/2, okRect_.y + 6, themes.text, FontStyle::MicroBold);
        
        // Cancel Button
        SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
        SDL_RenderFillRect(renderer, &cancelRect_);
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
        SDL_RenderDrawRect(renderer, &cancelRect_);
        int canW = fontMgr_.getLogicalWidth("Cancel", cat->ptSize(FontStyle::MicroBold));
        cat->drawText(renderer, "Cancel", cancelRect_.x + (cancelRect_.w - canW)/2, cancelRect_.y + 6, themes.text, FontStyle::MicroBold);
        
        return;
    }
    
    if (config_.powerwallUrl.empty()) {
        fontMgr_.catalog()->drawText(renderer, "Click to config", x_ + 10, y_ + height_/2, themes.textDim, FontStyle::SmallRegular);
        return;
    }
    if (!data_ || !data_->valid) {
        fontMgr_.catalog()->drawText(renderer, "Connecting...", x_ + 10, y_ + height_/2, themes.textDim, FontStyle::SmallRegular);
        return;
    }
    
    uint32_t now = SDL_GetTicks();
    if (lastRenderTicks_ == 0) lastRenderTicks_ = now;
    
    float dt = 0.0f;
    if (now - lastRenderTicks_ >= 1000) {
        dt = (now - lastRenderTicks_) / 1000.0f;
        lastRenderTicks_ = now;
    }
    
    int cx = x_ + width_ / 2;
    int cy = y_ + height_ / 2 + 5;
    
    int r = std::min(width_, height_) / 3.5f;
    float scale = std::max(1.0f, std::min(width_, height_) / 300.0f);
    
    // Node coordinates
    int sx = cx;
    int sy = cy - r;
    int gx = cx - r;
    int gy = cy;
    int hx = cx + r;
    int hy = cy;
    int bx = cx;
    int by = cy + r;

    // Line connection points (outside node radii)
    int offset = 22 * scale;
    int scy = sy + offset;
    int bcy = by - offset;
    int gcx = gx + offset;
    int hcx = hx - offset;

    SDL_Color solarColor = {255, 200, 0, 255};
    SDL_Color gridColor = {150, 150, 150, 255};
    SDL_Color batteryColor = {0, 255, 0, 255};
    SDL_Color homeColor = {0, 150, 255, 255};

    double raw_s = std::abs(data_->solarPower) < 100 ? 0 : data_->solarPower;
    double raw_h = std::abs(data_->homePower) < 100 ? 0 : data_->homePower;
    double s = std::max(0.0, raw_s);
    double h = std::max(0.0, raw_h);
    
    double bat = std::abs(data_->batteryPower) < 100 ? 0 : data_->batteryPower;
    // Tesla API: Negative = Charging, Positive = Discharging
    double b_charge = bat < 0 ? -bat : 0;
    double b_discharge = bat > 0 ? bat : 0;
    
    double grid = std::abs(data_->gridPower) < 100 ? 0 : data_->gridPower;
    double g_import = grid > 0 ? grid : 0;
    double g_export = grid < 0 ? -grid : 0;

    double s_to_h = 0, s_to_b = 0, s_to_g = 0;
    double b_to_h = 0, b_to_g = 0;
    double g_to_h = 0, g_to_b = 0;

    if (s > 0) {
        s_to_h = std::min(s, h); s -= s_to_h; h -= s_to_h;
        s_to_b = std::min(s, b_charge); s -= s_to_b; b_charge -= s_to_b;
        s_to_g = std::min(s, g_export); s -= s_to_g; g_export -= s_to_g;
    }
    if (b_discharge > 0) {
        b_to_h = std::min(b_discharge, h); b_discharge -= b_to_h; h -= b_to_h;
        b_to_g = std::min(b_discharge, g_export); b_discharge -= b_to_g; g_export -= b_to_g;
    }
    if (g_import > 0) {
        g_to_h = std::min(g_import, h); g_import -= g_to_h; h -= g_to_h;
        g_to_b = std::min(g_import, b_charge); g_import -= g_to_b; b_charge -= g_to_b;
    }

    // Draw very thin static central cross
    RenderUtils::drawThickLine(renderer, sx, scy, cx, cy, 1.0f * scale, solarColor);
    RenderUtils::drawThickLine(renderer, gcx, gy, cx, cy, 1.0f * scale, gridColor);
    RenderUtils::drawThickLine(renderer, bx, bcy, cx, cy, 1.0f * scale, batteryColor);
    RenderUtils::drawThickLine(renderer, cx, cy, hcx, hy, 1.0f * scale, homeColor);
    
    // Draw animated bezier flows
    drawBezierFlow(renderer, sx, scy, cx, cy, hcx, hy, s_to_h, solarColor, scale, solarProgress_, dt);
    drawBezierFlow(renderer, sx, scy, cx, cy, bx, bcy, s_to_b, solarColor, scale, solarProgress_, dt);
    drawBezierFlow(renderer, sx, scy, cx, cy, gcx, gy, s_to_g, solarColor, scale, solarProgress_, dt);

    drawBezierFlow(renderer, bx, bcy, cx, cy, hcx, hy, b_to_h, batteryColor, scale, batteryProgress_, dt);
    drawBezierFlow(renderer, bx, bcy, cx, cy, gcx, gy, b_to_g, batteryColor, scale, batteryProgress_, dt);

    drawBezierFlow(renderer, gcx, gy, cx, cy, hcx, hy, g_to_h, gridColor, scale, gridProgress_, dt);
    drawBezierFlow(renderer, gcx, gy, cx, cy, bx, bcy, g_to_b, gridColor, scale, gridProgress_, dt);
    
    // Draw Nodes
    drawHANode(renderer, themes, sx, sy, raw_s, "\xE2\x98\x80", solarColor, scale, false, 0);
    drawHANode(renderer, themes, gx, gy, grid, "\xE2\x9A\xA1", gridColor, scale, false, 0);
    drawHANode(renderer, themes, hx, hy, raw_h, "\xE2\x8C\x82", homeColor, scale, false, 0);
    drawHANode(renderer, themes, bx, by, bat, "", batteryColor, scale, true, data_->batteryLevel);
    
    // Draw interval indicator in bottom right
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%ds", config_.powerwallPollInterval);
    auto* cat = fontMgr_.catalog();
    int iw = fontMgr_.getLogicalWidth(buf, cat->ptSize(FontStyle::SmallRegular));
    cat->drawText(renderer, buf, x_ + width_ - iw - 5, y_ + height_ - 15, themes.textDim, FontStyle::SmallRegular);
}

void PowerwallPanel::drawBezierFlow(SDL_Renderer* renderer, int x0, int y0, int x1, int y1, int x2, int y2, double power, SDL_Color color, float scale, float& progressState, float dt) {
    if (power < 10) return;
    double speed = std::min(5.0, power / 1000.0) * 0.3; 
    
    int segments = 32;
    std::vector<SDL_FPoint> points;
    for (int i = 0; i <= segments; ++i) {
        float t = i / (float)segments;
        float u = 1.0f - t;
        float px = u*u*x0 + 2*u*t*x1 + t*t*x2;
        float py = u*u*y0 + 2*u*t*y1 + t*t*y2;
        points.push_back({px, py});
    }
    
    RenderUtils::drawPolyline(renderer, points.data(), points.size(), 1.5f * scale, color);
    
    // Accumulate time so changing speed doesn't teleport the dots
    progressState += speed * 1.5f * dt;
    float progress = fmod(progressState, 1.0f);
    
    for (int i = 0; i < 2; ++i) {
        float t = fmod(progress + (i / 2.0f), 1.0f);
        float u = 1.0f - t;
        float px = u*u*x0 + 2*u*t*x1 + t*t*x2;
        float py = u*u*y0 + 2*u*t*y1 + t*t*y2;
        RenderUtils::drawCircle(renderer, px, py, 2.0f * scale, color);
    }
}

void PowerwallPanel::drawHANode(SDL_Renderer* renderer, const ThemeColors& themes, int cx, int cy, double power, const std::string& icon, SDL_Color color, float scale, bool isBattery, double batLevel) {
    RenderUtils::drawArcOutline(renderer, cx, cy, 22.0f * scale, 0.0f, 360.0f, 1.0f * scale, color);
    
    auto* cat = fontMgr_.catalog();
    
    // We want to center the [Icon + Value] block.
    // Total height is roughly 16 (icon) + 4 (gap) + 12 (text) = 32.
    // Top of block is cy - 16.
    int iconY = cy - (16 * scale);
    if (isBattery) iconY = cy - (6 * scale); // Shift battery down to fit percent above
    
    if (isBattery) {
        int bw = 18 * scale;
        int bh = 10 * scale;
        int bx = cx - bw / 2;
        int by = iconY + 2 * scale;
        
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        
        // Draw 2px thick battery outline
        SDL_Rect outline1 = {bx, by, bw, bh};
        SDL_Rect outline2 = {bx - 1, by - 1, bw + 2, bh + 2};
        SDL_RenderDrawRect(renderer, &outline1);
        SDL_RenderDrawRect(renderer, &outline2);
        
        // Draw terminal on the right
        SDL_Rect terminal = {bx + bw, by + bh / 4, static_cast<int>(2 * scale) + 1, bh / 2};
        SDL_RenderFillRect(renderer, &terminal);
        
        // Draw dynamic fill
        int fillW = std::max(0, static_cast<int>((bw - 4) * (batLevel / 100.0)));
        if (fillW > 0) {
            SDL_Rect fill = {bx + 2, by + 2, fillW, bh - 4};
            SDL_RenderFillRect(renderer, &fill);
        }
    } else {
        float lw = fontMgr_.getLogicalWidth(icon, cat->ptSize(FontStyle::SmallRegular));
        cat->drawText(renderer, icon, cx - lw/2 + (1 * scale), iconY, color, FontStyle::SmallRegular);
    }
    
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f kW", power / 1000.0);
    int pw = fontMgr_.getLogicalWidth(buf, cat->ptSize(FontStyle::Micro));
    cat->drawText(renderer, buf, cx - pw/2, iconY + (18 * scale), color, FontStyle::Micro);
    
    if (isBattery) {
        std::snprintf(buf, sizeof(buf), "%.1f %%", batLevel);
        int percentW = fontMgr_.getLogicalWidth(buf, cat->ptSize(FontStyle::Micro));
        cat->drawText(renderer, buf, cx - percentW/2, cy - (20 * scale), color, FontStyle::Micro);
    }
}

bool PowerwallPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
    LOG_D("PowerwallPanel", "PowerwallPanel::onMouseDown mx = {}, my = {}, mod = {}, clicks = {}", mx, my, mod, clicks);
    if (isConfiguring_) {
        if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y && my < okRect_.y + okRect_.h) {
            config_.powerwallUrl = urlInput_.getValue();
            ConfigManager::instance().save(config_);
            isConfiguring_ = false;
            return true;
        }
        if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w && my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
            isConfiguring_ = false;
            return true;
        }
        return true;
    }

    auto* cat = fontMgr_.catalog();
    int iw = fontMgr_.getLogicalWidth("300s", cat->ptSize(FontStyle::SmallRegular));
    if (mx >= x_ + width_ - iw - 15 && my >= y_ + height_ - 25) {
        int ints[] = {10, 30, 60, 300};
        for (int i = 0; i < 4; i++) {
            if (config_.powerwallPollInterval == ints[i]) {
                config_.powerwallPollInterval = ints[(i + 1) % 4];
                return true;
            }
        }
        config_.powerwallPollInterval = 10;
        return true;
    } else {
        isConfiguring_ = true;
        urlInput_.setValue(config_.powerwallUrl);
        urlInput_.setCursorPos(config_.powerwallUrl.length());
        
        int mw = std::min(220, width_ - 8);
        int mh = 100;
        modalRect_ = { x_ + (width_ - mw) / 2, y_ + (height_ - mh) / 2, mw, mh };
        urlRect_ = { modalRect_.x + 4, modalRect_.y + 25, mw - 8, 24 };
        int btnW = std::min(80, (mw - 12) / 2);
        okRect_ = { modalRect_.x + 4, modalRect_.y + 65, btnW, 26 };
        cancelRect_ = { modalRect_.x + mw - btnW - 4, modalRect_.y + 65, btnW, 26 };
        return true;
    }
}

bool PowerwallPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
    if (isConfiguring_) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            config_.powerwallUrl = urlInput_.getValue();
            ConfigManager::instance().save(config_);
            isConfiguring_ = false;
            return true;
        } else if (key == SDLK_ESCAPE) {
            isConfiguring_ = false;
            return true;
        }
        return urlInput_.onKeyDown(key, mod);
    }
    return false;
}

bool PowerwallPanel::onTextInput(const char* text) {
    if (isConfiguring_) {
        return urlInput_.onTextInput(text);
    }
    return false;
}
