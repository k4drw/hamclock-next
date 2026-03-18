#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <SDL.h>
#include <mutex>
#include <string>

#include <memory>

class CloudProvider : public ProviderBase, public std::enable_shared_from_this<CloudProvider> {
public:
  CloudProvider(NetworkManager &netMgr);
  ~CloudProvider();

  void update();

  bool hasData() const;
  const std::string &getData() const;
  uint32_t getLastUpdateMs() const { return lastUpdateMs_; }

  // Returns a texture representing the cloud data, created/cached for the given renderer.
  // Returns nullptr if no data is available.
  SDL_Texture *getTexture(SDL_Renderer *renderer, int w, int h, SDL_Color tint = {255, 255, 255, 255});

  // Forces destruction of the cached GPU texture so it will be rebuilt
  // from the next decoded surface. Call update() after to queue re-decode.
  void invalidateTexture();

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
