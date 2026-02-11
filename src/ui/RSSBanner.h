#pragma once

#include "FontManager.h"
#include "Widget.h"
#include "../core/RSSData.h"

#include <memory>
#include <string>
#include <vector>

class RSSBanner : public Widget {
public:
    RSSBanner(int x, int y, int w, int h,
              FontManager& fontMgr,
              std::shared_ptr<RSSDataStore> store);
    ~RSSBanner() override { destroyCache(); }

    void update() override;
    void render(SDL_Renderer* renderer) override;
    void onResize(int x, int y, int w, int h) override;

private:
    void destroyCache();
    void rebuildTextures(SDL_Renderer* renderer);

    FontManager& fontMgr_;
    std::shared_ptr<RSSDataStore> store_;

    // Scroll state
    float scrollOffset_ = 0.0f;
    Uint32 lastTick_ = 0;

    // Per-headline textures (headline, separator, headline, separator, ...)
    struct Entry {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };
    std::vector<Entry> entries_;
    int totalWidth_ = 0;  // sum of all entry widths
    int maxHeight_ = 0;   // tallest entry

    // Track when headlines change
    std::vector<std::string> lastHeadlines_;

    // Font size for the banner
    int fontSize_ = 33;

    static constexpr float kScrollSpeed = 60.0f; // pixels per second
    static constexpr const char* kSeparator = " - ";
};
