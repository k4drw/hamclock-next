#pragma once

#include "Widget.h"
#include "../core/NetLoggerStore.h"
#include "../core/NetLoggerStore.h"
#include "FontManager.h"
#include <SDL2/SDL.h>

class NetLoggerPanel : public Widget {
public:
    NetLoggerPanel(int x, int y, int w, int h, FontManager& fontMgr, std::shared_ptr<NetLoggerStore> store);
    ~NetLoggerPanel() override = default;

    void render(SDL_Renderer* renderer) override;
    void update() override;
    
    bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
    bool onMouseWheel(int scrollY) override;
    void onMouseMove(int mx, int my) override;
    

    std::shared_ptr<const NetLoggerData> data_;
    FontManager& fontMgr_;
    std::shared_ptr<NetLoggerStore> store_;
    
    int scrollY_ = 0;
    int maxScroll_ = 0;
    int rowHeight_ = 40;
    
    int mx_ = 0;
    int my_ = 0;
    
    bool overlayActive_ = true;
    
    SDL_Rect contentRect_;
    
    void renderNetList(SDL_Renderer* renderer, const ThemeColors& themes);
    void renderCheckinList(SDL_Renderer* renderer, const ThemeColors& themes);
};
