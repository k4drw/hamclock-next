#pragma once

#include "../network/NetworkManager.h"
#include <SDL.h>
#include <mutex>
#include <string>
#include <vector>

struct GribField {
  std::vector<float> values;
  int nx = 0, ny = 0;
};

// ---- Output types for GPU rendering ----------------------------------------
struct WxSegment {
  float lon1, lat1;
  float lon2, lat2;
};

struct WxQuiver {
  float lon, lat;
  float u, v;
};
// ---------------------------------------------------------------------------

class WxMbProvider {
public:
  explicit WxMbProvider(NetworkManager &net);
  ~WxMbProvider();

  void update();

  // Copy decoded segments, quivers, and optionally a pressure fill surface.
  // fillSurface is non-null only when fresh GFS data arrived; caller owns it.
  // Returns true when new data was available (caller should rebuild GPU
  // buffers).
  bool getSegments(std::vector<WxSegment> &segs, std::vector<WxQuiver> &quivers,
                   SDL_Surface *&fillSurface);

  // Force geometry rebuild on next getSegments() (e.g. projection change).
  // Note: fillSurface will be nullptr on rebuild-only calls (no new GFS data).
  void invalidate() {
    std::lock_guard<std::mutex> lk(mutex_);
    segmentsDirty_ = true;
  }

  bool hasData() const;
  uint64_t getLastUpdateMs() const;

private:
  static bool decodeGFS(const std::vector<uint8_t> &data, GribField &prmsl,
                        GribField &ugrd, GribField &vgrd);

  // Run marching squares + quivers → lat/lon geometry, and build fill surface.
  static void buildSegments(const GribField &prmsl, const GribField &ugrd,
                            const GribField &vgrd, float pMin, float pMax,
                            int W, int H, std::vector<WxSegment> &segs,
                            std::vector<WxQuiver> &quivers,
                            SDL_Surface *&fillSurface);

  static std::string buildNomadsUrl();

  NetworkManager &net_;

  std::vector<WxSegment> segments_;
  std::vector<WxQuiver> quivers_;
  SDL_Surface *pendingFillSurface_ = nullptr; // 360x180 RGBA pressure fill
  bool segmentsDirty_ = false;
  bool hasData_ = false;
  uint64_t lastUpdateMs_ = 0;
  std::string lastUrl_;
  std::string fetchingUrl_;

  mutable std::mutex mutex_;
};
