#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <SDL.h>
#include <memory>
#include <mutex>
#include <string>

// Fetches GFS total cloud cover (TCDC) from NOMADS and decodes the GRIB2
// into a 360x180 RGBA SDL_Surface (white pixels, alpha = coverage).
class GribCloudProvider
    : public ProviderBase, public std::enable_shared_from_this<GribCloudProvider> {
public:
  explicit GribCloudProvider(NetworkManager &net);
  ~GribCloudProvider();

  // Trigger a fetch if the GFS cycle URL has changed since last fetch.
  void update();

  // Return the most recently decoded surfaces and clear the pending vector.
  // Returns empty vector if no new data since the last call. Caller owns the
  // returned SDL_Surfaces and must SDL_FreeSurface() them when done.
  std::vector<SDL_Surface*> takeSurfaces();

  bool hasData() const;

private:
  static std::string buildUrl(int fhr);
  static SDL_Surface *decodeGrib(const std::vector<uint8_t> &data);

  NetworkManager &net_;
  std::vector<SDL_Surface*> pendingSurfaces_;
  bool hasData_ = false;
  std::string lastUrl_;
  std::string fetchingUrl_;
  mutable std::mutex mutex_;
};
