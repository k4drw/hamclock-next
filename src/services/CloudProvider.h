#pragma once

#include "../network/NetworkManager.h"
#include <SDL.h>
#include <mutex>
#include <string>

class CloudProvider {
public:
  CloudProvider(NetworkManager &netMgr);
  ~CloudProvider();

  void update();

  bool hasData() const;
  const std::string &getData() const;
  uint32_t getLastUpdateMs() const { return lastUpdateMs_; }

  // Returns a texture representing the cloud data, created/cached for the given renderer.
  // Returns nullptr if no data is available.
  SDL_Texture *getTexture(SDL_Renderer *renderer, int w, int h);

private:
  NetworkManager &netMgr_;
  bool hasData_ = false;
  uint32_t lastUpdateMs_ = 0;
  mutable std::mutex mutex_;

  SDL_Texture *texture_ = nullptr;
  SDL_Surface *pendingSurface_ = nullptr;
  int texW_ = 0;
  int texH_ = 0;
  bool textureDirty_ = false;
};
