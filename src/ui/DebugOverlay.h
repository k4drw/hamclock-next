#pragma once

#include "FontCatalog.h"
#include "FontManager.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Reference rectangle from hamclock_layout.md at logical 800x480.
struct SpecRect {
    const char* name;
    int x, y, w, h;
};

static constexpr int kLogicalW = 800;
static constexpr int kLogicalH = 480;

// All major zones from hamclock_layout.md.
static const SpecRect kSpecRects[] = {
    {"Callsign/Title",  0,   0,   230,  52},
    {"Clock Face",      0,   65,  230,  49},
    {"Aux Time",        0,   113, 204,  32},
    {"Pane 1",          235, 0,   160, 148},
    {"Pane 2",          405, 0,   160, 148},
    {"Pane 3",          575, 0,   160, 148},
    {"NCDXF/Status",    738, 0,    62, 148},
    {"Side Panel",      0,   148, 139, 332},
    {"DE Info",         1,   185, 137, 109},
    {"DX Info",         1,   295, 137, 184},
    {"Map Box",         139, 149, 660, 330},
    {"RSS Banner",      139, 412, 660,  68},
};
static constexpr int kNumSpecRects = sizeof(kSpecRects) / sizeof(kSpecRects[0]);

// An actual widget's name + rectangle (in window coordinates).
struct WidgetRect {
    std::string name;
    SDL_Rect rect;
};

class DebugOverlay {
public:
    explicit DebugOverlay(FontManager& fontMgr) : fontMgr_(fontMgr) {}

    void toggle() { visible_ = !visible_; }
    bool isVisible() const { return visible_; }

    // Draw spec rects (yellow) and actual widget rects (cyan) on top of the scene.
    void render(SDL_Renderer* renderer, int winW, int winH,
                const std::vector<WidgetRect>& actuals)
    {
        if (!visible_) return;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        float sx = static_cast<float>(winW) / kLogicalW;
        float sy = static_cast<float>(winH) / kLogicalH;

        // --- Spec rects: yellow double outline ---
        for (int i = 0; i < kNumSpecRects; ++i) {
            const auto& s = kSpecRects[i];
            SDL_Rect r = {
                static_cast<int>(s.x * sx),
                static_cast<int>(s.y * sy),
                static_cast<int>(s.w * sx),
                static_cast<int>(s.h * sy),
            };

            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
            SDL_RenderDrawRect(renderer, &r);
            SDL_Rect inner = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
            SDL_RenderDrawRect(renderer, &inner);

            char label[128];
            std::snprintf(label, sizeof(label), "[S] %s %d,%d %dx%d",
                          s.name, s.x, s.y, s.w, s.h);
            drawLabel(renderer, label, r.x + 3, r.y + 3, {255, 255, 0, 255});
        }

        // --- Actual widget rects: cyan outline ---
        for (const auto& a : actuals) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 200);
            SDL_RenderDrawRect(renderer, &a.rect);

            // Convert to logical 800x480 for label
            int lx = static_cast<int>(a.rect.x / sx);
            int ly = static_cast<int>(a.rect.y / sy);
            int lw = static_cast<int>(a.rect.w / sx);
            int lh = static_cast<int>(a.rect.h / sy);

            char label[128];
            std::snprintf(label, sizeof(label), "[A] %s %d,%d %dx%d",
                          a.name.c_str(), lx, ly, lw, lh);
            // Place at bottom of rect to avoid overlapping spec labels
            int labelY = a.rect.y + a.rect.h - 14;
            if (labelY < a.rect.y + 16) labelY = a.rect.y + 16;
            drawLabel(renderer, label, a.rect.x + 3, labelY, {0, 255, 255, 255});
        }

        // --- Font calibration panel (bottom-left) ---
        auto* cat = fontMgr_.catalog();
        if (cat) {
            auto calib = cat->calibrate();
            int cy = winH - 18 - static_cast<int>(calib.size()) * 14;
            drawLabel(renderer, "Font Calibration:", 6, cy - 16,
                      {255, 200, 0, 255});
            for (const auto& e : calib) {
                char line[128];
                int delta = e.measuredHeight - e.targetHeight;
                std::snprintf(line, sizeof(line),
                    "  %-14s  tgt=%2dpx  base=%2dpt  scl=%2dpt  meas=%2dpx  %+dpx",
                    e.name, e.targetHeight, e.basePt, e.scaledPt,
                    e.measuredHeight, delta);
                SDL_Color c = (std::abs(delta) <= 3)
                    ? SDL_Color{0, 255, 0, 255}
                    : SDL_Color{255, 100, 100, 255};
                drawLabel(renderer, line, 6, cy, c);
                cy += 14;
            }
        }

        // Legend at bottom-right
        drawLabel(renderer, "Yellow=[S]pec  Cyan=[A]ctual  Press 'O' to toggle",
                  winW - 340, winH - 18, {255, 255, 255, 255});
    }

    // Print a delta report to stderr comparing spec vs actual positions.
    void dumpReport(int winW, int winH,
                    const std::vector<WidgetRect>& actuals) const
    {
        float sx = static_cast<float>(winW) / kLogicalW;
        float sy = static_cast<float>(winH) / kLogicalH;

        std::fprintf(stderr,
            "\n========== LAYOUT DELTA REPORT (window %dx%d) ==========\n\n",
            winW, winH);

        std::fprintf(stderr, "Spec rects (hamclock_layout.md, logical 800x480):\n");
        for (int i = 0; i < kNumSpecRects; ++i) {
            const auto& s = kSpecRects[i];
            std::fprintf(stderr, "  %-16s  (%3d,%3d) %3dx%3d\n",
                         s.name, s.x, s.y, s.w, s.h);
        }

        std::fprintf(stderr, "\nActual widget rects (converted to logical 800x480):\n");
        for (const auto& a : actuals) {
            int lx = static_cast<int>(a.rect.x / sx);
            int ly = static_cast<int>(a.rect.y / sy);
            int lw = static_cast<int>(a.rect.w / sx);
            int lh = static_cast<int>(a.rect.h / sy);
            std::fprintf(stderr, "  %-16s  (%3d,%3d) %3dx%3d\n",
                         a.name.c_str(), lx, ly, lw, lh);
        }

        // Semantic mapping: spec index → actual widget index
        struct Mapping { int specIdx; int actualIdx; };
        static const Mapping mappings[] = {
            {0, 0},   // Callsign/Title  → TimePanel
            {3, 1},   // Pane 1          → SpaceWx
            {4, 2},   // Pane 2          → DX Cluster
            {5, 3},   // Pane 3          → LiveSpots
            {6, 4},   // NCDXF/Status    → Band Cond
            {7, 5},   // Side Panel      → LocalPanel (top half)
            {9, 6},   // DX Info         → DXSatPane
            {10, 7},  // Map Box         → MapWidget
            {11, 8},  // RSS Banner      → RSSBanner
        };

        std::fprintf(stderr,
            "\nDelta (spec -> actual, tolerance +-2px = MATCH):\n");
        std::fprintf(stderr,
            "  %-16s  %-16s  %-17s  %-17s  %s\n",
            "Spec Zone", "Widget", "Spec", "Actual", "Status");
        std::fprintf(stderr,
            "  %-16s  %-16s  %-17s  %-17s  %s\n",
            "--------", "------", "----", "------", "------");

        for (const auto& m : mappings) {
            if (m.specIdx >= kNumSpecRects ||
                m.actualIdx >= static_cast<int>(actuals.size()))
                continue;

            const auto& s = kSpecRects[m.specIdx];
            const auto& a = actuals[m.actualIdx];

            int lx = static_cast<int>(a.rect.x / sx);
            int ly = static_cast<int>(a.rect.y / sy);
            int lw = static_cast<int>(a.rect.w / sx);
            int lh = static_cast<int>(a.rect.h / sy);

            int dx = lx - s.x, dy = ly - s.y;
            int dw = lw - s.w, dh = lh - s.h;
            bool match = (std::abs(dx) <= 2 && std::abs(dy) <= 2 &&
                          std::abs(dw) <= 2 && std::abs(dh) <= 2);

            char specStr[24], actStr[24];
            std::snprintf(specStr, sizeof(specStr), "(%d,%d %dx%d)",
                          s.x, s.y, s.w, s.h);
            std::snprintf(actStr, sizeof(actStr), "(%d,%d %dx%d)",
                          lx, ly, lw, lh);

            if (match) {
                std::fprintf(stderr, "  %-16s  %-16s  %-17s  %-17s  MATCH\n",
                             s.name, a.name.c_str(), specStr, actStr);
            } else {
                char delta[48];
                std::snprintf(delta, sizeof(delta), "DIFF x%+d y%+d w%+d h%+d",
                              dx, dy, dw, dh);
                std::fprintf(stderr, "  %-16s  %-16s  %-17s  %-17s  %s\n",
                             s.name, a.name.c_str(), specStr, actStr, delta);
            }
        }

        std::fprintf(stderr,
            "\n  Unmapped spec zones: Clock Face, Aux Time, DE Info\n"
            "  (Sub-zones within existing widgets)\n");
        // Font calibration
        auto* cat = fontMgr_.catalog();
        if (cat) {
            std::fprintf(stderr, "\nFont Calibration:\n");
            std::fprintf(stderr, "  %-14s  %6s  %6s  %6s  %8s  %5s\n",
                         "Style", "Target", "BasePt", "SclPt", "Measured", "Delta");
            auto calib = cat->calibrate();
            for (const auto& e : calib) {
                int delta = e.measuredHeight - e.targetHeight;
                std::fprintf(stderr, "  %-14s  %4dpx  %4dpt  %4dpt  %6dpx  %+3dpx\n",
                             e.name, e.targetHeight, e.basePt, e.scaledPt,
                             e.measuredHeight, delta);
            }
        }

        std::fprintf(stderr,
            "\n========== END DELTA REPORT ==========\n\n");
    }

private:
    void drawLabel(SDL_Renderer* renderer, const char* text, int x, int y,
                   SDL_Color fg, int ptSize = 10)
    {
        TTF_Font* font = fontMgr_.getFont(ptSize);
        if (!font) return;

        int tw = 0, th = 0;
        TTF_SizeText(font, text, &tw, &th);

        // Dark background for readability
        SDL_Rect bg = {x - 1, y - 1, tw + 2, th + 2};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(renderer, &bg);

        fontMgr_.drawText(renderer, text, x, y, fg, ptSize);
    }

    FontManager& fontMgr_;
    bool visible_ = false;
};
