#pragma once

#include "Widget.h"
#include "../core/PowerwallStore.h"
#include "../core/ConfigManager.h"
#include <memory>
#include <string>
#include "TextInput.h"

class PowerwallPanel : public Widget {
public:
    PowerwallPanel(int x, int y, int w, int h, FontManager& fontMgr, std::shared_ptr<PowerwallStore> store, AppConfig& config);
    
    void update() override;
    void render(SDL_Renderer* renderer) override;
    bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
    bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
    bool onTextInput(const char* text) override;

    bool isModalActive() const override { return isConfiguring_; }
    bool isConfiguring() const override { return isConfiguring_; }

private:
    void drawHANode(SDL_Renderer* renderer, const ThemeColors& themes, int cx, int cy, double power, const std::string& icon, SDL_Color color, float scale, bool isBattery, double batLevel);
    void drawBezierFlow(SDL_Renderer* renderer, int x0, int y0, int x1, int y1, int x2, int y2, double power, SDL_Color color, float scale, float& progressState, float dt);

    FontManager& fontMgr_;
    std::shared_ptr<PowerwallStore> store_;
    AppConfig& config_;
    std::shared_ptr<const PowerwallData> data_;
    
    uint32_t animTick_ = 0;
    
    // Smooth animation states to prevent jerking when speed changes
    uint32_t lastRenderTicks_ = 0;
    float solarProgress_ = 0.0f;
    float gridProgress_ = 0.0f;
    float homeProgress_ = 0.0f;
    float batteryProgress_ = 0.0f;
    
    bool isConfiguring_ = false;
    TextInput urlInput_;
    SDL_Rect modalRect_ = {0, 0, 0, 0};
    SDL_Rect urlRect_ = {0, 0, 0, 0};
    SDL_Rect okRect_ = {0, 0, 0, 0};
    SDL_Rect cancelRect_ = {0, 0, 0, 0};
};
