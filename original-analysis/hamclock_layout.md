# HamClock Layout and Dimensions

This document defines the pixel-perfect layout of the HamClock user interface based on the internal source code definitions. These coordinates are based on the **800 x 480** logical resolution.

## Coordinate System
*   **Logical Resolution:** 800 x 480 pixels.
*   **Origin:** Top-Left (0, 0).
*   **Scaling:** All coordinates are multiplied by `SCALESZ` when rendered to physical displays.

## 1. Top Bar (Status & Plots)
The top section of the screen is approximately 148 pixels high and contains the clock, callsign, and configurable plot panes.

| Zone Name | X | Y | Width | Height | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Callsign/Title** | 0 | 0 | 230 | 52 | Main Callsign display (`cs_info.box`). |
| **Clock Face** | 0 | 65 | 230 | 49 | Digital or Analog clock (`clock_b`). |
| **Aux Time** | 0 | 113 | 204 | 32 | Date or secondary time (`auxtime_b`). |
| **Stopwatch Icon** | 149 | 93 | 38 | 22 | Stopwatch control icon (`stopwatch_b`). |
| **Lock Icon** | 217 | 118 | 11 | 16 | Touch lock indicator (`lkscrn_b`). |
| **Pane 1** | 235 | 0 | 160 | 148 | Configurable Plot 1 (`plot_b[1]`). |
| **Pane 2** | 405 | 0 | 160 | 148 | Configurable Plot 2 (`plot_b[2]`). |
| **Pane 3** | 575 | 0 | 160 | 148 | Configurable Plot 3 (`plot_b[3]`). |
| **NCDXF / Status** | 738 | 0 | 62 | 148 | NCDXF Beacons, Space Wx, or Brightness (`NCDXF_b`). |

*Note: There are 10-pixel gaps between Panes 1, 2, and 3 which act as separators.*

## 2. Main Map Area (Bottom Right)
The largest single element, typically occupied by the world map.

| Zone Name | X | Y | Width | Height | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Map Box** | 139 | 149 | 660 | 330 | The main map viewport (`map_b`). |
| **View Button** | 139 | 149 | 60 | 14 | "View" button overlay (top-left of map). |
| **RSS Banner** | 139 | 412 | 660 | 68 | Scrolling RSS news tape (overlays bottom of map). |
| **Map Scale** | 139 | *Dynamic* | 660 | 10 | Scale bar. Y depends on RSS banner state. |

*   **Map Dimensions:** `EARTH_W` (660) x `EARTH_H` (330).
*   **Borders:** Lines are drawn at `y = 148` (Top border of map/sidebar) and around the edges.

## 3. Side Panel (Bottom Left)
This column (Pane 0) displays DE (Home) and DX (Target) information. It can be overlaid by a full-height plot if "Pane 0" is used for data.

| Zone Name | X | Y | Width | Height | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Pane 0 (Overlay)**| 0 | 148 | 139 | 332 | Full sidebar area (`plot_b[0]`). Used when overriding DE/DX info. |
| **DE Title** | 1 | 153 | 30 | 30 | "DE:" Label (`de_title_b`). Approx Y position. |
| **DE Info Box** | 1 | 185 | 137 | 109 | DE Location/Sun info (`de_info_b`). |
| **DE Maidenhead** | 1 | *Calc* | 68 | *Calc* | Within DE Info box. |
| **DX Info Box** | 1 | 295 | 137 | 184 | DX Location/Path info (`dx_info_b`). |
| **DX Title** | 1 | 325 | - | - | "DX:" Label (Approx Y). |
| **Satellite Name** | 1 | 296 | 137 | 30 | Satellite name overlay in DX box (`satname_b`). |

*   **DE/DX Split:** Horizontal line usually separates DE and DX info around Y = 294.
*   **Info Rows:** The DE and DX info boxes are further subdivided into text rows using `devspace` and `dxvspace` calculations.

## 4. Derived & Dynamic Boxes
Some layouts are calculated at runtime:
*   **Maidenhead Labels:** `maidlbltop_b` (Top of map) and `maidlblright_b` (Right of map).
*   **WiFi / Version / Uptime:** Small status boxes appearing just below the main Callsign in the top-left area (`uptime_b`, `version_b`, `wifi_b`).

## Summary of Major Zones

```text
(0,0)
+------------------------+-----------+-----------+-----------+-------+
|  Callsign / Clock      |  Pane 1   |  Pane 2   |  Pane 3   | NCDXF |
|  (235 x 148)           | (160x148) | (160x148) | (160x148) |(62x148)|
+------------------------+-----------+-----------+-----------+-------+ (y=148)
|                        |                                           |
|  Side Panel (Pane 0)   |             Main Map Area                 |
|  (DE / DX Info)        |             (660 x 330)                   |
|  (139 x 332)           |                                           |
|                        |                                           |
|                        +-------------------------------------------+ (y=480)
+------------------------+ (x=800)
```
