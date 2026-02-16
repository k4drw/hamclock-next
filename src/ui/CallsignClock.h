#pragma once

#include "FontManager.h"
#include "Widget.h"

#include <string>

struct SDL_Renderer;
struct SDL_Texture;

class CallsignClock : public Widget {
public:
    CallsignClock(int x, int y, int w, int h,
                  FontManager& fontMgr, const std::string& callsign)
        : Widget(x, y, w, h)
        , fontMgr_(fontMgr)
        , callsign_(callsign) {}

    ~CallsignClock() override { destroyCache(); }

    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache();

    FontManager& fontMgr_;
    std::string callsign_;

    // Cached textures: callsign (static), time (changes per second), date (changes per day)
    SDL_Texture* callTex_ = nullptr;
    int callW_ = 0, callH_ = 0;

    SDL_Texture* timeTex_ = nullptr;
    int timeW_ = 0, timeH_ = 0;
    std::string lastTime_;

    SDL_Texture* dateTex_ = nullptr;
    int dateW_ = 0, dateH_ = 0;
    std::string lastDate_;

    std::string currentTime_;
    std::string currentDate_;

    int callFontSize_ = 24;
    int timeFontSize_ = 16;
    int dateFontSize_ = 12;
    int lastCallFontSize_ = 0;
    int lastTimeFontSize_ = 0;
    int lastDateFontSize_ = 0;
};
