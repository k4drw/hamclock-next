#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

struct SDL_Renderer;
struct SDL_Surface;

// Captures the SDL renderer output as JPEG frames for MJPEG streaming.
// capture() must be called from the main (SDL) thread.
// waitFrame() is safe to call from any other thread.
class FrameCapture {
public:
  FrameCapture();
  ~FrameCapture();

  // Start the background encoding worker.
  // Must be called after construction and before capture().
  void start();

  // Capture the current renderer contents as a JPEG frame.
  // Must be called from the main thread (before or after SDL_RenderPresent).
  void capture(SDL_Renderer *renderer);

  // Block until a frame with seq > afterSeq is available or timeoutMs elapses.
  // Writes the new seq into outSeq. Returns empty vector on timeout.
  std::vector<uint8_t> waitFrame(uint64_t afterSeq, int timeoutMs,
                                 uint64_t &outSeq) const;

  // Return the current frame sequence number (non-blocking).
  uint64_t latestSeq() const;

  // Cap capture rate. 0 = no cap (default).
  void setMaxFps(int fps);

  // Connection tracking
  void addSubscriber() { subscribers_++; }
  void removeSubscriber() {
    if (subscribers_ > 0)
      subscribers_--;
  }
  bool hasSubscribers() const { return subscribers_ > 0; }

  int quality = 70; // JPEG quality 1-100

private:
  void workerLoop();

  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;
  std::vector<uint8_t> jpegData_;
  uint64_t seq_{0};
  int maxFps_ = 0;
  uint32_t lastCaptureMs_ = 0;
  std::atomic<int> subscribers_{0};

  // Async encoding
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> busy_{false};
  SDL_Surface *pendingSurface_ = nullptr;
  SDL_Surface *cachedSurface_ = nullptr;
  std::mutex workerMutex_;
  std::condition_variable workerCv_;
};
