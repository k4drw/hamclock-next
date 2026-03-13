#pragma once

#include "../network/NetworkManager.h"
#include <SDL.h>
#include <memory>
#include <mutex>
#include <string>

// Fetches GFS total cloud cover (TCDC) from NOMADS and decodes the GRIB2
// into a 360x180 RGBA SDL_Surface (white pixels, alpha = coverage).
class GribCloudProvider
    : public std::enable_shared_from_this<GribCloudProvider> {
public:
  explicit GribCloudProvider(NetworkManager &net);
  ~GribCloudProvider();

  // Trigger a fetch if the GFS cycle URL has changed since last fetch.
  void update();

  // Return the most recently decoded surface and clear the pending pointer.
  // Returns nullptr if no new data since the last call.  Caller owns the
  // returned SDL_Surface and must SDL_FreeSurface() it when done.
  SDL_Surface *takeSurface();

  bool hasData() const;

private:
  static std::string buildUrl();
  static SDL_Surface *decodeGrib(const std::vector<uint8_t> &data);

  NetworkManager &net_;
  SDL_Surface *pendingSurface_ = nullptr;
  bool hasData_ = false;
  std::string lastUrl_;
  std::string fetchingUrl_;
  mutable std::mutex mutex_;
};
