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

class WxMbProvider {
public:
    explicit WxMbProvider(NetworkManager& net);
    ~WxMbProvider();

    // Trigger fetch of current GFS cycle; no-op if cycle URL unchanged.
    void update();

    // Returns cached SDL_Texture (RGBA, BLEND) scaled to (w, h), or nullptr.
    // Must be called from the main/render thread.
    SDL_Texture* getTexture(SDL_Renderer* renderer, int w, int h);

    bool hasData() const;
    uint64_t getLastUpdateMs() const;

private:
    // Decode raw GRIB2 bytes into three fields (PRMSL hPa, UGRD m/s, VGRD m/s).
    // Returns true only if all three fields decoded with simple packing.
    static bool decodeGFS(const std::vector<uint8_t>& data,
                          GribField& prmsl, GribField& ugrd, GribField& vgrd);

    // Render pressure contours + wind quivers to a new RGBA SDL_Surface (w×h).
    // Caller owns the returned surface.
    static SDL_Surface* renderToSurface(const GribField& prmsl,
                                        const GribField& ugrd,
                                        const GribField& vgrd,
                                        int w, int h);

    // Build the NOAA NOMADS GFS filter URL for the current best cycle.
    static std::string buildNomadsUrl();

    NetworkManager& net_;

    SDL_Surface*  pendingSurface_ = nullptr; // produced by WorkerService, consumed by getTexture()
    SDL_Texture*  texture_        = nullptr;
    bool          dirty_          = false;
    bool          hasData_        = false;
    uint64_t      lastUpdateMs_   = 0;
    int           texW_           = 0;
    int           texH_           = 0;
    std::string   lastUrl_;

    mutable std::mutex mutex_;
};
