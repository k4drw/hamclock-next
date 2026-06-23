#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <SDL.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

// Fetches NHC wind speed probabilities (e.g., tpcprblty...grib2.gz) and
// provides animated map overlays of tropical storm probabilities.
class TropicsGribProvider
    : public ProviderBase, public std::enable_shared_from_this<TropicsGribProvider> {
public:
  explicit TropicsGribProvider(NetworkManager &net);
  ~TropicsGribProvider();

  // Trigger a fetch if the cycle URL has changed since last fetch.
  void update();

  // Return the most recently decoded surfaces and clear the pending vector.
  // Returns empty vector if no new data since the last call. Caller owns the
  // returned SDL_Surfaces and must SDL_FreeSurface() them when done.
  std::vector<SDL_Surface*> takeSurfaces();

  bool hasData() const;

private:
  static std::string buildUrl();
  static std::vector<uint8_t> decompressGz(const std::string &compressed);
  static SDL_Surface *decodeGrib(const std::vector<uint8_t> &data, int forecastHour);

  NetworkManager &net_;
  std::vector<SDL_Surface*> pendingSurfaces_;
  bool hasData_ = false;
  std::string lastUrl_;
  std::string fetchingUrl_;
  mutable std::mutex mutex_;
};
