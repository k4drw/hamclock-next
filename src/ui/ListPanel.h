#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <SDL_pixels.h>

#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

class ListPanel : public Widget {
public:
  ListPanel(int x, int y, int w, int h, FontManager &fontMgr,
            const std::string &title, const std::vector<std::string> &rows);
  ~ListPanel() override { destroyCache(); }

  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void setRows(const std::vector<std::string> &rows);
  void setHighlightedIndex(int index) { highlightedIndex_ = index; }
  int getHighlightedIndex() const { return highlightedIndex_; }

  virtual SDL_Color getRowColor(int /*index*/,
                                const SDL_Color &defaultColor) const {
    return defaultColor;
  }

  std::string getName() const override { return "ListPanel:" + title_; }
  nlohmann::json getDebugData() const override;

protected:
  void destroyCache();

  FontManager &fontMgr_;
  std::string title_;
  std::vector<std::string> rows_;
  int highlightedIndex_ = -1;

  SDL_Texture *titleTex_ = nullptr;
  int titleW_ = 0, titleH_ = 0;

  struct RowCache {
    SDL_Texture *tex = nullptr;
    int w = 0, h = 0;
    std::string text;
  };
  std::vector<RowCache> rowCache_;

  int titleFontSize_ = 12;
  int rowFontSize_ = 10;
  int lastTitleFontSize_ = 0;
  int lastRowFontSize_ = 0;
};
