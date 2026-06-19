#include "IonosondePanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "../core/ConfigManager.h"
#include "../core/Astronomy.h"
#include <algorithm>

namespace HamClock {

IonosondePanel::IonosondePanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) { (void)texMgr; }

void IonosondePanel::updateData(const IonosondeData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

void IonosondePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  cat->drawText(renderer, "Live Ionosonde", x_ + 10, y_ + 5, themes.accent, FontStyle::MicroBold);

  IonosondeData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid || d.stations.empty()) {
    cat->drawText(renderer, "Waiting for DIDBase...", x_ + width_/2, y_ + height_/2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  auto config = ConfigManager::instance().getConfig();
  LatLon de = {config.lat, config.lon};

  auto sortedStations = d.stations;
  std::sort(sortedStations.begin(), sortedStations.end(), [&de](const IonosondeStation& a, const IonosondeStation& b) {
    return Astronomy::calculateDistance(de, {a.lat, a.lon}) < Astronomy::calculateDistance(de, {b.lat, b.lon});
  });

  int numClosest = std::min(2, (int)sortedStations.size());
  float avgFof2 = 0;
  for (int i = 0; i < numClosest; ++i) {
    avgFof2 += sortedStations[i].foF2;
  }
  avgFof2 /= numClosest;

  int curY = fontMgr_.catalog()->ptSize(FontStyle::MicroBold) + 10 + y_;
  int pad = 10;

  // Overview stats
  cat->drawText(renderer, numClosest > 1 ? "Avg foF2 (Local):" : "foF2 (Local):", x_ + pad, curY, themes.text, FontStyle::Fast);
  
  curY += 15;
  
  SDL_Color idxCol = themes.success; // Green
  if (avgFof2 < 5.0f) idxCol = themes.warning; // Yellow
  if (avgFof2 < 3.0f) idxCol = themes.danger; // Red
  
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f MHz", avgFof2);
  cat->drawText(renderer, buf, x_ + pad, curY, idxCol, FontStyle::Fast);
  
  curY += 22;
  
  for (int i = 0; i < numClosest; ++i) {
    const auto& st = sortedStations[i];
    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", st.name.c_str());
    cat->drawText(renderer, nameBuf, x_ + pad, curY, themes.accent, FontStyle::Micro);
    curY += 15;
    
    std::snprintf(nameBuf, sizeof(nameBuf), "fof2: %.1f MHz", st.foF2);
    cat->drawText(renderer, nameBuf, x_ + pad, curY, themes.accent, FontStyle::Micro);
    curY += 15;

    char distBuf[64];
    double dist = Astronomy::calculateDistance(de, {st.lat, st.lon});
    std::string distUnit = "km";
    if (!useMetric_) {
      dist *= 0.621371;
      distUnit = "mi";
    }
    std::snprintf(distBuf, sizeof(distBuf), "%.0f %s  hmF2: %.0f km", dist, distUnit.c_str(), st.hmF2.value_or(0.0));
    cat->drawText(renderer, distBuf, x_ + pad + 10, curY, themes.textDim, FontStyle::Tiny);
    curY += 17;
  }
}

void IonosondePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void IonosondePanel::onMouseMove(int mx, int my) {
  (void)mx; (void)my;
}


} // namespace HamClock

REGISTER_WIDGET("ionosonde", "Ionosonde", false, false, {
  return std::make_unique<HamClock::IonosondePanel>(0, 0, 0, 0, deps.fontMgr, deps.texMgr);
})
