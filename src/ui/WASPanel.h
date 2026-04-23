#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/ADIFData.h"
#include <memory>
#include <string>
#include <vector>

class WASPanel : public Widget {
public:
    WASPanel(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<ADIFStore> store);

    std::string getName() const override { return "WAS"; }
    const char *typeId() const override { return "was_progress"; }
    std::string getDisplayName() const override { return "WAS Progress"; }

    void update() override;
    void render(SDL_Renderer *renderer) override;

private:
    FontManager &fontMgr_;
    std::shared_ptr<ADIFStore> store_;
    ADIFStats stats_;
    
    struct StateEntry {
        std::string abbr;
        bool worked = false;
        bool confirmed = false;
    };
    std::vector<StateEntry> states_;
    
    void initStates();
    void updateStates();
};
