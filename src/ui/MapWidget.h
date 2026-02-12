#pragma once

#include "../core/DXClusterData.h"
#include "../core/HamClockState.h"
#include "../core/LiveSpotData.h"
#include "../core/OrbitPredictor.h"
#include "../network/NetworkManager.h"
#include "TextureManager.h"
#include "Widget.h"

#include <SDL2/SDL.h>

#include <memory>
#include <mutex>
#include <string>

class MapWidget : public Widget {
public:
  MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
            NetworkManager &netMgr, std::shared_ptr<HamClockState> state)
      : Widget(x, y, w, h), texMgr_(texMgr), netMgr_(netMgr),
        state_(std::move(state)) {}

  ~MapWidget() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

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

private:
  SDL_FPoint latLonToScreen(double lat, double lon) const;
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

  TextureManager &texMgr_;
  NetworkManager &netMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<LiveSpotDataStore> spotStore_;
  std::shared_ptr<DXClusterDataStore> dxcStore_;
  OrbitPredictor *predictor_ = nullptr;

  SDL_Rect mapRect_ = {};
  bool mapLoaded_ = false;
  int currentMonth_ = 0; // 1-12

  std::mutex mapDataMutex_;
  std::string pendingMapData_;

  double sunLat_ = 0;
  double sunLon_ = 0;
};
