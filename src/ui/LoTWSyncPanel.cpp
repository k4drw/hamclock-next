#include "LoTWSyncPanel.h"
#include "WidgetRegistry.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <sstream>

LoTWSyncPanel::LoTWSyncPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : ListPanel(x, y, w, h, fontMgr, "LoTW Sync", {}) {}

LoTWSyncPanel::~LoTWSyncPanel() { clearCache(); }

void LoTWSyncPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  clearCache();
}

void LoTWSyncPanel::clearCache() {
  auto &mm = MemoryMonitor::getInstance();
  if (syncTimeCache_.tex) mm.destroyTexture(syncTimeCache_.tex);
  if (qsoCountCache_.tex) mm.destroyTexture(qsoCountCache_.tex);
  if (statusCache_.tex) mm.destroyTexture(statusCache_.tex);
  if (errorCache_.tex) mm.destroyTexture(errorCache_.tex);
  if (configMsgCache_.tex) mm.destroyTexture(configMsgCache_.tex);

  syncTimeCache_ = {};
  qsoCountCache_ = {};
  statusCache_ = {};
  errorCache_ = {};
  configMsgCache_ = {};
}

void LoTWSyncPanel::setSyncStatus(time_t lastSync, int qsoCount, const std::string &error) {
  if (lastSyncTime_ != lastSync || qsosSynced_ != qsoCount || lastError_ != error) {
    lastSyncTime_ = lastSync;
    qsosSynced_ = qsoCount;
    lastError_ = error;
    // We don't clear the cache here because render() will check if the text changed.
    // However, for simplicity, we could just clear it. Let's let render() handle it.
  }
}

void LoTWSyncPanel::update() {
  rowH_ = std::max(12, static_cast<int>(height_ * 0.08f));
}

void LoTWSyncPanel::render(SDL_Renderer *renderer) {
  if (!renderer || !fontMgr_.ready())
    return;

  // Render title bar (handled by ListPanel)
  ListPanel::render(renderer);

  ThemeColors themes = getThemeColors(theme_);
  int pad = 4;
  int fontSize = std::max(10, rowH_ - 2);
  int lineH = rowH_;
  int textY = contentY_;

  // Helper to render or use cache
  auto renderOrCache = [&](StatusCache &cache, const std::string &text, SDL_Color color, int fSize) {
    bool colorChanged = (cache.lastColor.r != color.r || cache.lastColor.g != color.g ||
                         cache.lastColor.b != color.b || cache.lastColor.a != color.a);
    if (!cache.tex || cache.lastText != text || colorChanged) {
      if (cache.tex)
        MemoryMonitor::getInstance().destroyTexture(cache.tex);
      cache.tex = fontMgr_.renderText(renderer, text, color, fSize, &cache.w, &cache.h);
      cache.lastText = text;
      cache.lastColor = color;
    }
    if (cache.tex) {
      SDL_Rect dst = {x_ + pad, textY, cache.w, cache.h};
      SDL_RenderCopy(renderer, cache.tex, nullptr, &dst);
      textY += lineH;
    }
  };

  // If no data synced yet, show config message
  if (lastSyncTime_ == 0 && lastError_.empty()) {
    int msgFontSize = std::max(10, height_ / 15);
    renderOrCache(configMsgCache_,
                  "Configure in Setup > Services\nwith LoTW callsign & password",
                  themes.textDim, msgFontSize);
    return;
  }

  // Status line
  std::ostringstream ss;
  if (lastSyncTime_ == 0) {
    ss << "Status: Never synced";
  } else {
    auto tm = std::gmtime(&lastSyncTime_);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", tm);
    ss << "Last sync: " << buf;
  }
  renderOrCache(syncTimeCache_, ss.str(), themes.text, fontSize);

  // QSOs synced count
  ss.str("");
  ss << "QSOs synced: " << qsosSynced_;
  renderOrCache(qsoCountCache_, ss.str(), themes.text, fontSize);

  // Connection status
  ss.str("");
  ss << "Status: " << (lastError_.empty() ? "Connected" : "Error");
  SDL_Color statusColor = lastError_.empty() ? themes.success : themes.danger;
  renderOrCache(statusCache_, ss.str(), statusColor, fontSize);

  // Error message if present
  if (!lastError_.empty()) {
    int errSize = std::max(8, fontSize - 1);
    renderOrCache(errorCache_, lastError_, themes.danger, errSize);
  }
}

REGISTER_WIDGET("lotw_sync", "LoTW Sync", false, false, {
  return std::make_unique<LoTWSyncPanel>(0, 0, 0, 0, deps.fontMgr);
});
