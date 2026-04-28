#pragma once

#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/HeardMeStore.h"
#include "MapContext.h"
#include "Widget.h"
#include <memory>

class HeardMeStore;
struct HamClockState;
struct AppConfig;
class TextureManager;

class HeardMePanel : public Widget {
public:
  HeardMePanel(int x, int y, int w, int h, FontManager &fontMgr,
               TextureManager &texMgr,
               std::shared_ptr<HeardMeStore> store, AppConfig *config,
               std::shared_ptr<HamClockState> state);
  ~HeardMePanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  // Renders the Heard Me spots on the world map.
  // This is called by MapWidget during its render pass.
  void renderMapOverlay(SDL_Renderer *renderer, const MapContext &ctx);

  std::string getName() const override { return "HeardMe"; }
  const char *typeId() const override { return "heard_me"; }
  std::string getDisplayName() const override { return "Heard Me"; }
  bool isMapOverlayActive() const { return true; }

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<HeardMeStore> store_;
  AppConfig *config_;
  std::shared_ptr<HamClockState> state_;

  std::vector<DXClusterSpot> lastSpots_;
  std::chrono::system_clock::time_point lastUpdate_{};

  struct CachedRow {
    SDL_Texture *callTex = nullptr;
    int callW = 0, callH = 0;
    SDL_Texture *snrTex = nullptr;
    int snrW = 0, snrH = 0;
    SDL_Texture *wpmTex = nullptr;
    int wpmW = 0, wpmH = 0;
    SDL_Texture *ageTex = nullptr;
    int ageW = 0, ageH = 0;
    std::string lastAge;
  };
  std::vector<CachedRow> cache_;
  void clearCache();

  std::string formatAge(const std::chrono::system_clock::time_point &spottedAt) const;
};
