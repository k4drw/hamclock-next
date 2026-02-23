#include "FrameCapture.h"
#include "../core/Logger.h"

#include <SDL.h>
#include <SDL_image.h>
#include <chrono>
#include <cstring>

// ---------------------------------------------------------------------------
// Growing-buffer SDL_RWops — allows IMG_SaveJPG_RW to write to a std::vector
// without a fixed size limit.
// ---------------------------------------------------------------------------

struct GrowBuf {
  std::vector<uint8_t> data;
  size_t pos = 0;
};

static Sint64 gb_size(SDL_RWops *ctx) {
  return static_cast<Sint64>(
      static_cast<GrowBuf *>(ctx->hidden.unknown.data1)->data.size());
}

static Sint64 gb_seek(SDL_RWops *ctx, Sint64 offset, int whence) {
  auto *g = static_cast<GrowBuf *>(ctx->hidden.unknown.data1);
  Sint64 newPos;
  if (whence == RW_SEEK_SET)
    newPos = offset;
  else if (whence == RW_SEEK_CUR)
    newPos = static_cast<Sint64>(g->pos) + offset;
  else
    newPos = static_cast<Sint64>(g->data.size()) + offset;
  g->pos = static_cast<size_t>(std::max<Sint64>(0, newPos));
  return static_cast<Sint64>(g->pos);
}

static size_t gb_read(SDL_RWops *ctx, void *ptr, size_t size, size_t maxn) {
  auto *g = static_cast<GrowBuf *>(ctx->hidden.unknown.data1);
  if (size == 0 || maxn == 0 || g->pos >= g->data.size())
    return 0;
  size_t avail = (g->data.size() - g->pos) / size;
  size_t n = std::min(avail, maxn);
  std::memcpy(ptr, g->data.data() + g->pos, n * size);
  g->pos += n * size;
  return n;
}

static size_t gb_write(SDL_RWops *ctx, const void *ptr, size_t size,
                       size_t num) {
  auto *g = static_cast<GrowBuf *>(ctx->hidden.unknown.data1);
  size_t bytes = size * num;
  size_t end = g->pos + bytes;
  if (end > g->data.size())
    g->data.resize(end);
  std::memcpy(g->data.data() + g->pos, ptr, bytes);
  g->pos = end;
  return num;
}

static int gb_close(SDL_RWops * /*ctx*/) { return 0; }

static SDL_RWops *makeGrowBufRW(GrowBuf *g) {
  SDL_RWops *rw = SDL_AllocRW();
  if (!rw)
    return nullptr;
  rw->size = gb_size;
  rw->seek = gb_seek;
  rw->read = gb_read;
  rw->write = gb_write;
  rw->close = gb_close;
  rw->hidden.unknown.data1 = g;
  return rw;
}

// ---------------------------------------------------------------------------

void FrameCapture::capture(SDL_Renderer *renderer) {
  int w, h;
  SDL_GetRendererOutputSize(renderer, &w, &h);
  if (w <= 0 || h <= 0)
    return;

  SDL_Surface *surf =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
  if (!surf)
    return;

  if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24,
                           surf->pixels, surf->pitch) != 0) {
    SDL_FreeSurface(surf);
    return;
  }

  GrowBuf gb;
  SDL_RWops *rw = makeGrowBufRW(&gb);
  if (!rw) {
    SDL_FreeSurface(surf);
    return;
  }

  if (IMG_SaveJPG_RW(surf, rw, 0, quality) == 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    jpegData_ = std::move(gb.data);
    ++seq_;
    cv_.notify_all();
  } else {
    LOG_W("FrameCapture", "JPEG encode failed: {}", IMG_GetError());
  }

  SDL_FreeRW(rw);
  SDL_FreeSurface(surf);
}

std::vector<uint8_t> FrameCapture::waitFrame(uint64_t afterSeq, int timeoutMs,
                                              uint64_t &outSeq) const {
  std::unique_lock<std::mutex> lock(mutex_);
  bool ready = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this, afterSeq] { return seq_ > afterSeq; });
  outSeq = seq_;
  if (ready && !jpegData_.empty())
    return jpegData_; // copy under lock
  return {};
}

uint64_t FrameCapture::latestSeq() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return seq_;
}
