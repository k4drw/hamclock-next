#pragma once

#include <cstdint>

// Lightweight base class for network-fetching providers.
// Providers set lastFetchMs_ (via SDL_GetTicks()) when a fetch is initiated
// or completes, enabling DashboardContext to call isStale() instead of
// maintaining its own per-provider timer variables.
class ProviderBase {
public:
  virtual ~ProviderBase() = default;

  // Returns true if no fetch has occurred or the last fetch was more than
  // intervalMs milliseconds ago (relative to nowMs = SDL_GetTicks()).
  bool isStale(uint32_t nowMs, uint32_t intervalMs) const {
    return lastFetchMs_ == 0 || (nowMs - lastFetchMs_) > intervalMs;
  }

  uint32_t lastFetchTime() const { return lastFetchMs_; }

protected:
  uint32_t lastFetchMs_ = 0;
};
