#include "VoacapDeDxPanel.h"
#include "WidgetRegistry.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// BANDS_MHZ and BANDS_STR are defined in the header, now we explicitly
// instantiate
constexpr double VoacapDeDxPanel::BANDS_MHZ[8];
constexpr const char *VoacapDeDxPanel::BANDS_STR[8];

VoacapDeDxPanel::VoacapDeDxPanel(
    int x, int y, int w, int h, FontManager &fontMgr,
    std::shared_ptr<HamClockState> state,
    std::shared_ptr<SolarDataStore> solarStore,
    std::shared_ptr<IonosondeProvider> ionoProvider,
    AppConfig &config)
    : Widget(x, y, w, h),
      fontMgr_(fontMgr),
      state_(std::move(state)),
      solarStore_(std::move(solarStore)),
      ionoProvider_(std::move(ionoProvider)),
      config_(config) {
  for (int i = 0; i < 24; ++i) {
    for (int j = 0; j < 8; ++j) {
      relMatrix_[i][j] = 0.0f;
    }
  }
}

void VoacapDeDxPanel::recalculateMatrix() {
  hasTarget_ = state_->dxActive;
  if (!hasTarget_)
    return;

  double marginDb =
      PropEngine::calculateSignalMargin(config_.propMode, (double)config_.propPower);

  // Space weather
  double sfi = 70.0;
  double ssn = 50.0;
  double kIndex = 0.0;
  if (solarStore_) {
    auto sw = solarStore_->get();
    if (sw.sfi > 0)
      sfi = sw.sfi;
    if (sw.sunspot_number > 0)
      ssn = sw.sunspot_number;
    kIndex = sw.k_index;
  }

  // Calculate mid-point
  double phi1 = state_->deLocation.lat * M_PI / 180.0;
  double lam1 = state_->deLocation.lon * M_PI / 180.0;
  double phi2 = state_->dxLocation.lat * M_PI / 180.0;
  double lam2 = state_->dxLocation.lon * M_PI / 180.0;

  double Bx = std::cos(phi2) * std::cos(lam2 - lam1);
  double By = std::cos(phi2) * std::sin(lam2 - lam1);
  double midPhi = std::atan2(
      std::sin(phi1) + std::sin(phi2),
      std::sqrt((std::cos(phi1) + Bx) * (std::cos(phi1) + Bx) + By * By));
  double midLam = lam1 + std::atan2(By, std::cos(phi1) + Bx);

  double midLatDeg = midPhi * 180.0 / M_PI;
  double midLonDeg = midLam * 180.0 / M_PI;

  double distKm =
      Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
  if (config_.propPath == 1) {
    // Long path
    distKm = 40075.0 - distKm;
    // Note: for long path, the "midpoint" should ideally be shifted 180 degrees
    // along the great circle.
  }

  // Interpolate iono
  InterpolatedIonosonde iono;
  if (ionoProvider_) {
    iono = ionoProvider_->interpolate(midLatDeg, midLonDeg);
  }

  std::time_t t = std::time(nullptr);
  std::tm *ptm = std::gmtime(&t);
  double currentHour = ptm->tm_hour + ptm->tm_min / 60.0;

  // Compute 24 hours x 8 bands
  for (int hour = 0; hour < 24; ++hour) {
    for (int b = 0; b < 8; ++b) {
      double rel = PropEngine::calculateReliability(
          BANDS_MHZ[b], distKm, midLatDeg, midLonDeg, hour, sfi, ssn, kIndex,
          iono, currentHour, marginDb);
      
      // Apply TOA penalty if engine supported it directly, but for now we
      // just pass the parameters we have.
      
      relMatrix_[hour][b] = (float)rel;
    }
  }

  lastDeLat_ = state_->deLocation.lat;
  lastDeLon_ = state_->deLocation.lon;
  lastDxLat_ = state_->dxLocation.lat;
  lastDxLon_ = state_->dxLocation.lon;
  lastSFI_ = sfi;
  lastCalcHour_ = ptm->tm_hour;
  lastMode_ = config_.propMode;
  lastPower_ = config_.propPower;
  lastToa_ = config_.propToa;
  lastPath_ = config_.propPath;
}

void VoacapDeDxPanel::update() {
  std::time_t t = std::time(nullptr);
  std::tm *ptm = std::gmtime(&t);
  int currentHour = ptm->tm_hour;

  double currentSfi = solarStore_ ? solarStore_->get().sfi : 70.0;

  bool targetChanged = (state_->dxActive != hasTarget_) ||
                       (state_->dxLocation.lat != lastDxLat_) ||
                       (state_->dxLocation.lon != lastDxLon_) ||
                       (state_->deLocation.lat != lastDeLat_) ||
                       (state_->deLocation.lon != lastDeLon_);

  bool metricChanged =
      (currentSfi != lastSFI_) || (currentHour != lastCalcHour_);

  bool propChanged = (config_.propMode != lastMode_) ||
                     (config_.propPower != lastPower_) ||
                     (config_.propToa != lastToa_) ||
                     (config_.propPath != lastPath_);

  if (targetChanged || metricChanged || propChanged) {
    recalculateMatrix();
  }
}

static SDL_Color reliabilityColor(float rel) {
  // rel:    0     10         33         66          100
  // color: black   |   red    |  yellow  |   green
  if (rel < 10.0f) {
    return {0, 0, 0, 255};
  } else if (rel < 33.0f) {
    return {255, 0, 0, 255};
  } else if (rel < 66.0f) {
    return {255, 255, 0, 255};
  } else {
    return {0, 255, 0, 255};
  }
}

void VoacapDeDxPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  renderChrome(renderer);

  auto *cat = fontMgr_.catalog();

  int titleH = 20;
  cat->drawText(renderer, "VOACAP DE-DX", x_ + width_ / 2, y_ + 5,
                themes.accent, FontStyle::MicroBold, true, true);

  if (!hasTarget_) {
    cat->drawText(renderer, "Select target on map", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, themes.textDim,
                  FontStyle::Micro, true, true);
    return;
  }

  // Draw matrix
  // X axis: 24 hours
  // Y axis: 8 bands (0=80m at bottom, 7=10m at top)

  int pLeft = 36;
  int pRight = 16;
  int pTop = titleH + 5;
  int pBot = 40;  // for timeline and legend

  int pWidth = width_ - pLeft - pRight;
  int pHeight = height_ - pTop - pBot;

  float cellW = static_cast<float>(pWidth) / 24.0f;
  float cellH = static_cast<float>(pHeight) / 8.0f;

  if (cellW < 1.0f)
    cellW = 1.0f;
  if (cellH < 1.0f)
    cellH = 1.0f;

  std::time_t t = std::time(nullptr);
  std::tm *ptm = std::gmtime(&t);
  int utcHour = ptm->tm_hour;

  // The matrix rendering
  for (int hourOffset = 0; hourOffset < 24; ++hourOffset) {
    int h = (utcHour + hourOffset) % 24;
    int px = x_ + pLeft + static_cast<int>(hourOffset * cellW);

    for (int b = 0; b < 8; ++b) {
      int py = y_ + pTop + static_cast<int>((7 - b) * cellH);

      SDL_Color c = reliabilityColor(relMatrix_[h][b]);

      SDL_Rect rect = {px, py, static_cast<int>(cellW + 1),
                       static_cast<int>(cellH + 1)};
      SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
      SDL_RenderFillRect(renderer, &rect);
    }
  }

  // Y-axis labels (Bands)
  for (int b = 0; b < 8; ++b) {
    int py = y_ + pTop + static_cast<int>((7 - b) * cellH) +
             static_cast<int>(cellH / 2);
    cat->drawText(renderer, BANDS_STR[b], x_ + (pLeft / 2), py, themes.textDim,
                  FontStyle::Tiny, true, true);
  }

  // X-axis labels (Timeline)
  // Draw current UTC at offset 0
  int timelineY = y_ + pTop + pHeight + 8;
  cat->drawText(renderer, "UTC", x_ + (pLeft / 2), timelineY, themes.textDim,
                FontStyle::Tiny, true, true);

  for (int hourOffset = 0; hourOffset < 24; hourOffset += 4) {
    int h = (utcHour + hourOffset) % 24;
    int px = x_ + pLeft + static_cast<int>(hourOffset * cellW);
    std::string hStr = std::to_string(h);
    cat->drawText(renderer, hStr, px + static_cast<int>(cellW / 2), timelineY,
                  themes.textDim, FontStyle::Tiny, true, true);
  }

  // Legend
  int legendY = y_ + pTop + pHeight + 23;
  int legendItemW = pWidth / 4;
  const float relVals[4] = {0.0f, 20.0f, 50.0f, 80.0f};
  const char *legendText[4] = {"<10", "10-33", "33-66", ">66"};

  for (int i = 0; i < 4; i++) {
    SDL_Color c = reliabilityColor(relVals[i]);

    int lx = x_ + pLeft + i * legendItemW;
    SDL_Rect boxRect = {lx, legendY - 4, 8, 8};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &boxRect);

    cat->drawText(renderer, legendText[i], lx + 4, legendY + 6, themes.textDim,
                  FontStyle::Tiny, true, true);
  }

  // Grid lines mapping
  SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                         themes.rowStripe2.b, 255);
  for (int hourOffset = 0; hourOffset <= 24; ++hourOffset) {
    int px = x_ + pLeft + static_cast<int>(hourOffset * cellW);
    SDL_RenderDrawLine(renderer, px, y_ + pTop, px, y_ + pTop + pHeight);
  }
  for (int b = 0; b <= 8; ++b) {
    int pyy = y_ + pTop + static_cast<int>(b * cellH);
    SDL_RenderDrawLine(renderer, x_ + pLeft, pyy, x_ + pLeft + pWidth, pyy);
  }
}

void VoacapDeDxPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

SDL_Rect VoacapDeDxPanel::getActionRect(const std::string &action) const {
  (void)action;
  return {x_, y_, width_, height_};
}

nlohmann::json VoacapDeDxPanel::getDebugData() const {
  nlohmann::json j;
  j["has_target"] = hasTarget_;
  return j;
}

REGISTER_WIDGET("voacap_dedx", "Voacap DE-DX", false, false, {
  return std::make_unique<VoacapDeDxPanel>(0, 0, 0, 0, deps.fontMgr, deps.state,
                                           deps.solarStore,
                                           deps.ionosondeProvider, deps.appCfg);
})
