#pragma once

#include "../core/ADIFData.h"
#include "../core/ActivityData.h"
#include "../core/AuroraHistoryStore.h"
#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/HamClockState.h"
#include "../core/LiveSpotData.h"
#include "../core/OrbitPredictor.h"
#include "../network/NetworkManager.h"
#include "FontManager.h"
#include "MapViewMenu.h"
#include "TextureManager.h"
#include "Widget.h"

#include <SDL.h>

#include <memory>
#include <mutex>
#include <string>

class MufRtProvider;
class IonosondeProvider;
class SolarDataStore;

class MapWidget : public Widget {
public:
  MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
            FontManager &fontMgr, NetworkManager &netMgr,
            std::shared_ptr<HamClockState> state, AppConfig &config);

  ~MapWidget() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  void onMouseMove(int mx, int my) override;
  bool onMouseWheel(int scrollY) override;

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

  void setADIFStore(std::shared_ptr<ADIFStore> store) {
    adifStore_ = std::move(store);
  }

  void setActivityStore(std::shared_ptr<ActivityDataStore> store) {
    activityStore_ = std::move(store);
  }

  void setMufRtProvider(MufRtProvider *p) { mufrt_ = p; }
  void setIonosondeProvider(IonosondeProvider *p) { iono_ = p; }
  void setSolarDataStore(SolarDataStore *s) { solar_ = s; }

  void setOnConfigChanged(std::function<void()> cb) { onConfigChanged_ = cb; }

  // Modal interface for MapViewMenu
  bool isModalActive() const override;
  void renderModal(SDL_Renderer *renderer) override;

  // Semantic Debug API
  std::string getName() const override;
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

  // Thread-safe method for receiving data from background threads
  void onSatTrackReady(const std::vector<GroundTrackPoint>& track);
private:
  SDL_FPoint latLonToScreen(double lat, double lon) const;
  bool screenToLatLon(int sx, int sy, double &lat, double &lon) const;
  void recalcMapRect();
  void renderNightOverlay(SDL_Renderer *renderer);
  void renderGridOverlay(SDL_Renderer *renderer);
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
  void renderADIFPins(SDL_Renderer *renderer);
  void renderONTASpots(SDL_Renderer *renderer);
  void renderMufRtOverlay(SDL_Renderer *renderer);

  TextureManager &texMgr_;
  FontManager &fontMgr_;
  NetworkManager &netMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<LiveSpotDataStore> spotStore_;
  std::shared_ptr<DXClusterDataStore> dxcStore_;
  std::shared_ptr<AuroraHistoryStore> auroraStore_;
  std::shared_ptr<ADIFStore> adifStore_;
  std::shared_ptr<ActivityDataStore> activityStore_;
  MufRtProvider *mufrt_ = nullptr;
  IonosondeProvider *iono_ = nullptr;
  SolarDataStore *solar_ = nullptr;
  OrbitPredictor *predictor_ = nullptr;

  std::unique_ptr<MapViewMenu> mapViewMenu_;

  SDL_Rect mapRect_ = {};
  bool mapLoaded_ = false;
  int currentMonth_ = 0; // 1-12

  std::mutex mapDataMutex_;
  std::string pendingMapData_;
  std::string pendingNightMapData_;
  std::string pendingMufData_;

  double sunLat_ = 0;
  double sunLon_ = 0;
  uint32_t lastPosUpdateMs_ = 0;
  uint32_t lastSatTrackUpdateMs_ = 0;

  // Math caches to save CPU
  std::vector<LatLon> cachedGreatCircle_;
  std::vector<GroundTrackPoint> cachedSatTrack_;
  std::vector<SDL_Vertex> shadowVerts_;
  std::vector<SDL_Vertex> lightVerts_;
  std::vector<int> nightIndices_;

  // Caches for render-ready geometry to avoid per-frame recalculation
  bool greatCircleDirty_ = true;
  std::vector<SDL_Vertex> greatCircleVerts_;
  std::vector<int> greatCircleIndices_;
  bool satTrackDirty_ = true;
  std::vector<SDL_Vertex> satTrackVerts_;
  std::vector<int> satTrackIndices_;
  bool gridDirty_ = true;
  std::vector<SDL_Vertex> gridVerts_;

  // Buffers for batching spots (dynamic per frame)
  std::vector<SDL_Vertex> spotVerts_;
  std::vector<int> spotIndices_;
  std::vector<SDL_Vertex> mapVerts_;
  std::string lastProjection_;
  std::vector<SDL_Vertex> markerVerts_;
  std::vector<int> markerIndices_;

  LatLon lastDE_ = {0, 0};
  LatLon lastDX_ = {0, 0};

  // Tooltip state
  struct Tooltip {
    bool visible = false;
    std::string text;
    int x = 0;
    int y = 0;
    uint32_t timestamp = 0;
    // Cached texture to avoid per-frame creation/destruction
    SDL_Texture *cachedTexture = nullptr;
    std::string cachedText;
    int cachedW = 0;
    int cachedH = 0;
  } tooltip_;

  void renderTooltip(SDL_Renderer *renderer);
  void renderProjectionSelect(SDL_Renderer *renderer);

  AppConfig &config_;
  std::function<void()> onConfigChanged_;
  SDL_Rect projRect_ = {};
  bool useCompatibilityRenderPath_ = false;
  SDL_Texture *nightOverlayTexture_ = nullptr;
  SDL_Texture *mufRtTexture_ = nullptr;
  uint32_t lastMufUpdateMs_ = 0;
  double lastUpdateSunLat_ = -999.0;
  double lastUpdateSunLon_ = -999.0;

  void renderOverlayInfo(SDL_Renderer *renderer);
  void renderRssButton(SDL_Renderer *renderer);

  SDL_Rect rssRect_ = {};
};
