#pragma once

#include "ProviderBase.h"
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

#include <memory>

struct WxFrameData {
  std::vector<WxSegment> segments;
  std::vector<WxQuiver> quivers;
  SDL_Surface *fillSurface = nullptr;
};

class WxMbProvider : public ProviderBase, public std::enable_shared_from_this<WxMbProvider> {
public:
  explicit WxMbProvider(NetworkManager &net);
  ~WxMbProvider();

  void update();

  // Returns all decoded frames and clears the pending vector.
  // The caller owns the SDL_Surfaces inside WxFrameData.
  std::vector<WxFrameData> takeFrames();

  // Force geometry rebuild on next takeFrames() (e.g. projection change).
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

  static std::string buildNomadsUrl(int fhr);

  NetworkManager &net_;

  std::vector<WxFrameData> pendingFrames_;
  bool segmentsDirty_ = false;
  bool hasData_ = false;
  uint64_t lastUpdateMs_ = 0;
  std::string lastUrl_;
  std::string fetchingUrl_;

  mutable std::mutex mutex_;
};
