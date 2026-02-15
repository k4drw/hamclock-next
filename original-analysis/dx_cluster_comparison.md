# DX Cluster Logic Comparison: Original vs Next

This document compares the DX Cluster spot handling logic between the original HamClock and the `hamclock-next` implementation.

## Key Discrepancies

### 1. Prefix Resolution (Location Mapping)
*   **Original**: Uses `prefixes.cpp` which downloads a massive `cty-ll-dxcc.txt` file (800KB+) from the HamClock server. This file contains thousands of prefix-to-location mappings.
*   **Next**: Uses `PrefixManager` with a small hardcoded table of approximately 190 common prefixes. 
*   **Impact**: Many spots in `hamclock-next` do not show up on the map because their prefixes are not in the limited hardcoded table.

### 2. Spot List Display
*   **Original**: 
    *   Supports a scrollable list of spots.
    *   Shows Frequency, Callsign, and **Age** (e.g., "3m", "2h").
    *   Highlights Watchlist hits in red.
    *   Allows sorting by Frequency, Callsign, Grid, Time, or Distance.
*   **Next**: 
    *   Static `ListPanel` limited to 15 rows.
    *   No scrolling or sorting options.
    *   Shows Frequency and Callsign, but no age or watchlist highlights.
*   **Impact**: Lower information density and usability for monitoring many spots.

### 3. Map Visualization
*   **Original**:
    *   "Dithers" spot locations slightly if multiple spots are on the same prefix (using `ditherLL`).
    *   Draws different markers for TX (circle) and RX (square).
    *   Supports different label types (Callsign, Prefix, or just a Dot).
*   **Next**:
    *   Draws all spots on their exact prefix center point (stacking them).
    *   Uses simple markers.
*   **Impact**: Stacking makes it difficult to distinguish or click on multiple spots from the same region.

### 4. Connection Management
*   **Original**:
    *   Can configure separate "Spider", "AR", and "UDP" cluster types.
    *   Robust login and prompt detection.
*   **Next**:
    *   Unified `DXClusterProvider` handling Telnet and UDP.
    *   Simpler login logic.

## Recommended Improvements for hamclock-next

1.  **Port Prefix Database**: Implement downloading and parsing of `cty-ll-dxcc.txt` to improve location resolution.
2.  **Add Spot Age**: Update `DXClusterPanel` to calculate and display the age of each spot.
3.  **Implement Scrolling**: Update `ListPanel` to support scrolling if more than 15 spots are present.
4.  **Watchlist Highlighting**: Integrate `WatchlistStore` with `DXClusterPanel` to highlight matching rows.
5.  **Dither Map Spots**: Implement a dithering algorithm for the map to prevent perfect stacking.
