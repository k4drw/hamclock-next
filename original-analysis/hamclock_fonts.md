# HamClock Font Definitions

This document details the font sizes and styles used in HamClock, along with their internal definitions and scaling behavior. This information is intended to assist in creating a resizable SDL2 application with a matching look and feel.

## Base Resolution and Scaling

HamClock is designed around a fixed logical resolution, which is then scaled to fit the physical display.

*   **Base Logical Resolution:** 800 x 480 pixels
    *   `APP_WIDTH`: 800
    *   `APP_HEIGHT`: 480
*   **Scaling Factor (`SCALESZ`):**
    *   The application calculates a scaling factor based on the horizontal resolution: `SCALESZ = FB_XRES / APP_WIDTH`.
    *   All coordinates and font sizes in the code are generally defined in the 800x480 space and multiplied by `SCALESZ` when rendering to the framebuffer.

## Font Styles

HamClock uses a selection of bitmap fonts defined in C++ source files. The application aliases these specific fonts to abstract styles (Small, Large, etc.).

### 1. Small Font (`SMALL_FONT`)

Used for general text, labels, and smaller data displays.

*   **Internal Enum:** `SMALL_FONT`
*   **Usage:** Standard UI text. It has two weight variants.
*   **Bold Variant (`BOLD_FONT`):**
    *   **Source File:** `Germano-Bold-16.cpp`
    *   **GFX Object:** `Germano_Bold16pt7b`
    *   **Metrics:**
        *   `yAdvance` (Line Height): 43 pixels (in 800x480 space)
        *   Character Range: 0x20 - 0x7F (ASCII Space to Tilde)
*   **Light Variant (`LIGHT_FONT` - Default):**
    *   **Source File:** `Germano-Regular-16.cpp`
    *   **GFX Object:** `Germano_Regular16pt7b`
    *   **Metrics:**
        *   `yAdvance` (Line Height): 43 pixels (in 800x480 space)
        *   Character Range: 0x20 - 0x7F

### 2. Large Font (`LARGE_FONT`)

Used for primary time displays, callsigns, and major data points.

*   **Internal Enum:** `LARGE_FONT`
*   **Usage:** High-visibility text.
*   **Font Used:**
    *   **Source File:** `Germano-Bold-30.cpp`
    *   **GFX Object:** `Germano_Bold30pt7b`
    *   **Metrics:**
        *   `yAdvance` (Line Height): 80 pixels (in 800x480 space)
        *   Character Range: 0x20 - 0x7F

### 3. Fast Font (`FAST_FONT`)

*   **Internal Enum:** `FAST_FONT`
*   **Usage:** Typically used for very rapid updates or debug text where custom bitmap rendering overhead is to be avoided.
*   **Implementation:** Maps to `NULL` in `selectFontStyle`. In the context of `Adafruit_RA8875`, this typically triggers the use of the display controller's built-in hardware 5x7 pixel font (if available) or a default internal fallback font.

## implementation Notes for SDL2

To replicate HamClock's typography in an SDL2 resizable app:

1.  **Logical Coordinate System:** Implement a logical renderer size of 800x480 using `SDL_RenderSetLogicalSize`. This will handle the scaling of positions and primitive shapes automatically.
2.  **Font Loading:**
    *   You cannot directly use the C++ bitmap arrays. You will need to extract the bitmaps or, more practically, find the original "Germano" TrueType/OpenType font files.
    *   Load the fonts using `SDL_ttf`.
3.  **Font Sizing:**
    *   The "16pt" and "30pt" in the filenames are likely references to the creation process (e.g., using `fontconvert` tool) and may not map 1:1 to `SDL_ttf` point sizes due to DPI differences.
    *   **Target Heights:** Aim to match the `yAdvance` metrics relative to the 480px screen height.
        *   **Small Font:** Target a line height of approx **43px** relative to an 800x480 surface.
        *   **Large Font:** Target a line height of approx **80px** relative to an 800x480 surface.
    *   Since `SDL_ttf` sizes are often height-based, you might need to experiment. A rough starting point for `TTF_OpenFont` size arguments (assuming 72 DPI logic) might be:
        *   Small: ~32 pts
        *   Large: ~60 pts
    *   *Correction/Refinement:* The `yAdvance` of 43 and 80 are quite large for "16pt" and "30pt" fonts if interpreted strictly as typographic points. The bitmap generator likely produced large glyphs. In a direct 800x480 window, a 43px high font is nearly 10% of the screen height.
