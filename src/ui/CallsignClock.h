#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <string>

struct SDL_Renderer;
struct SDL_Texture;

class CallsignClock : public Widget {
public:
    CallsignClock(int x, int y, int w, int h,
                  FontManager& fontMgr, const std::string& callsign,
                  AppConfig& config)
        : Widget(x, y, w, h)
        , fontMgr_(fontMgr)
        , callsign_(callsign)
        , config_(config) {}

    ~CallsignClock() override { destroyCache(); }

    std::string getName() const override { return "CallsignClock"; }
    const char *typeId() const override { return "callsign_clock"; }
    std::string getDisplayName() const override { return "Callsign/Clock"; }
    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache();

    FontManager& fontMgr_;
    std::string callsign_;
    AppConfig& config_;

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
};
