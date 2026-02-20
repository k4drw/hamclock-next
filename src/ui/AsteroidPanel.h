#pragma once

#include "../services/AsteroidProvider.h"
#include "ListPanel.h"

class AsteroidPanel : public ListPanel {
public:
  AsteroidPanel(int x, int y, int w, int h, FontManager &fontMgr,
                AsteroidProvider &provider);
  ~AsteroidPanel() override = default;

  void update() override;
  void onResize(int x, int y, int w, int h) override;

  // Semantic Debug API
  std::string getName() const override { return "AsteroidPanel"; }

protected:
  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;

private:
  void rebuildRows();

  AsteroidProvider &provider_;
  AsteroidData lastData_;
};
