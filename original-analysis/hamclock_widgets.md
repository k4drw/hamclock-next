# HamClock Widget List

This document lists the available widgets (referred to as "Plots" or "Panes" in the codebase) available in HamClock.

## 1. Main Data Panes (Panes 1, 2, 3)
These widgets are available for the three side panels (Top, Middle, Bottom).

| Display Name | Internal Name | Description | Dependencies |
| :--- | :--- | :--- | :--- |
| **VOACAP_DEDX** | `PLOT_CH_BC` | VOACAP Area coverage map | |
| **DE_Wx** | `PLOT_CH_DEWX` | Weather at DE (Home) location | |
| **DX_Cluster** | `PLOT_CH_DXCLUSTER` | DX Cluster spots | Requires DX Cluster config |
| **DX_Wx** | `PLOT_CH_DXWX` | Weather at DX location | |
| **Solar_Flux** | `PLOT_CH_FLUX` | 10.7cm Solar Flux history | |
| **Planetary_K** | `PLOT_CH_KP` | Planetary K-index history | |
| **Moon** | `PLOT_CH_MOON` | Moon phase, elevation, and azimuth | |
| **NOAA_SpcWx** | `PLOT_CH_NOAASPW` | NOAA Space Weather scales (R, S, G) | |
| **Sunspot_N** | `PLOT_CH_SSN` | Sunspot Number history | |
| **X-Ray** | `PLOT_CH_XRAY` | Solar X-Ray Flux | |
| **Rotator** | `PLOT_CH_GIMBAL` | Rotator control and azimuth display | Component `haveGimbal()` |
| **ENV_Temp** | `PLOT_CH_TEMPERATURE` | Environmental Temperature | Requires BME280 sensor |
| **ENV_Press** | `PLOT_CH_PRESSURE` | Environmental Pressure | Requires BME280 sensor |
| **ENV_Humid** | `PLOT_CH_HUMIDITY` | Environmental Humidity | Requires BME280 sensor |
| **ENV_DewPt** | `PLOT_CH_DEWPOINT` | Environmental Dew Point | Requires BME280 sensor |
| **SDO** | `PLOT_CH_SDO` | Solar Dynamics Observatory images | |
| **Solar_Wind** | `PLOT_CH_SOLWIND` | Solar Wind speed and density | |
| **DRAP** | `PLOT_CH_DRAP` | D-Region Absorption Predictions | |
| **Countdown** | `PLOT_CH_COUNTDOWN` | User-defined countdown timer | Active only in Countdown mode |
| **Contests** | `PLOT_CH_CONTESTS` | List of current/upcoming contests | |
| **Live_Spots** | `PLOT_CH_PSK` | Live spots (PSKReporter, WSPR, etc.) | |
| **Bz_Bt** | `PLOT_CH_BZBT` | Interplanetary Magnetic Field components | |
| **On_The_Air** | `PLOT_CH_ONTA` | POTA (Parks on the Air) / SOTA spots | |
| **ADIF** | `PLOT_CH_ADIF` | Visualizer for ADIF logs | Requires ADIF file |
| **Aurora** | `PLOT_CH_AURORA` | Aurora forecast map | |
| **DXPeditions** | `PLOT_CH_DXPEDS` | List of active DX Peditions | |
| **Disturbance** | `PLOT_CH_DST` | Geomagnetic Disturbance (Dst index) | |

## 2. Map Overlays (Pane 0)
The main map area can also display specific widgets as an overlay (alternating with the map view or persistently).

*   **DX_Cluster**
*   **Contests**
*   **ADIF**
*   **On_The_Air**
*   **DXPeditions**

## 3. NCDXF Box Widgets (BRB Modes)
A smaller, rotating display area (typically the NCDXF Beacon box) can show these "mini-widgets".

| Display Name | Internal Name | Description |
| :--- | :--- | :--- |
| **NCDXF** | `BRB_SHOW_BEACONS` | NCDXF Beacon status and transmission schedule |
| **On/Off** | `BRB_SHOW_ONOFF` | Display on/off schedule times |
| **PhotoR** | `BRB_SHOW_PHOT` | Photocell resistance value |
| **Brite** | `BRB_SHOW_BR` | Screen brightness percentage |
| **Spc Wx** | `BRB_SHOW_SWSTATS` | Current Space Weather Stats (SFI, SSN, A, K) |
| **BME@76** | `BRB_SHOW_BME76` | Stats from BME280 sensor at address 0x76 |
| **BME@77** | `BRB_SHOW_BME77` | Stats from BME280 sensor at address 0x77 |
| **DX Wx** | `BRB_SHOW_DXWX` | Compact DX Weather display |
| **DE Wx** | `BRB_SHOW_DEWX` | Compact DE Weather display |
