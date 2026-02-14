#pragma once

#include "../core/AuroraHistoryStore.h"
#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/HamClockState.h"
#include "../core/LiveSpotData.h"
#include "../core/OrbitPredictor.h"
#include "../network/NetworkManager.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

#include <SDL.h>

#include <memory>
#include <mutex>
#include <string>

class MapWidget : public Widget {
public:
  MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
            FontManager &fontMgr, NetworkManager &netMgr,
            std::shared_ptr<HamClockState> state, const AppConfig &config);

  ~MapWidget() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  void onMouseMove(int mx, int my) override;

  // Set the satellite predictor for map overlays (non-owning). nullptr to
  // clear.
  void setPredictor(OrbitPredictor *pred) { predictor_ = pred; }

  // Set the live spot data store for map spot overlays.
  void setSpotStore(std::shared_ptr<LiveSpotDataStore> store) {
    spotStore_ = std::move(store);
  }

  void setDXClusterStore(std::shared_ptr<DXClusterDataStore> store) {
    dxcStore_ = std::move(store);
  }

  void setAuroraStore(std::shared_ptr<AuroraHistoryStore> store) {
    auroraStore_ = std::move(store);
  }

  // Semantic Debug API
  std::string getName() const override;
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  SDL_FPoint latLonToScreen(double lat, double lon) const;
  bool screenToLatLon(int sx, int sy, double &lat, double &lon) const;
  void recalcMapRect();
  void renderNightOverlay(SDL_Renderer *renderer);
  void renderGreatCircle(SDL_Renderer *renderer);
  enum class MarkerShape { Circle, Square };
  void renderMarker(SDL_Renderer *renderer, double lat, double lon, Uint8 r,
                    Uint8 g, Uint8 b, MarkerShape shape = MarkerShape::Circle,
                    bool outline = true);
  void renderSatellite(SDL_Renderer *renderer);
  void renderSatFootprint(SDL_Renderer *renderer, double lat, double lon,
                          double footprintKm);
  void renderSatGroundTrack(SDL_Renderer *renderer);
  void renderSpotOverlay(SDL_Renderer *renderer);
  void renderDXClusterSpots(SDL_Renderer *renderer);
  void renderAuroraOverlay(SDL_Renderer *renderer);

  TextureManager &texMgr_;
  FontManager &fontMgr_;
  NetworkManager &netMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<LiveSpotDataStore> spotStore_;
  std::shared_ptr<DXClusterDataStore> dxcStore_;
  std::shared_ptr<AuroraHistoryStore> auroraStore_;
  OrbitPredictor *predictor_ = nullptr;

  SDL_Rect mapRect_ = {};
  bool mapLoaded_ = false;
  int currentMonth_ = 0; // 1-12

  std::mutex mapDataMutex_;
  std::string pendingMapData_;
  std::string pendingNightMapData_;

  double sunLat_ = 0;
  double sunLon_ = 0;
  uint32_t lastPosUpdateMs_ = 0;

  // Math caches to save CPU
  std::vector<LatLon> cachedGreatCircle_;
  LatLon lastDE_ = {0, 0};
  LatLon lastDX_ = {0, 0};

  // Tooltip state
  struct Tooltip {
    bool visible = false;
    std::string text;
    int x = 0;
    int y = 0;
    uint32_t timestamp = 0;
  } tooltip_;

  void renderTooltip(SDL_Renderer *renderer);
  const AppConfig &config_;
  bool useCompatibilityRenderPath_ = false;
  SDL_Texture *nightOverlayTexture_ = nullptr;
  double lastUpdateSunLat_ = -999.0;
  double lastUpdateSunLon_ = -999.0;
};
