# Widgets Reference

HamClock-Next includes 45 widgets organized into six categories. Each pane can display any widget or cycle through a list of them.

To add a widget to a pane, click the **top strip** of the pane to open the widget picker.

![Widget selector](images/modal-widget-selector.png)

---

## Dashboard Rotation Gallery

Since HamClock-Next allows multiple widgets per pane, the following snapshots show various dashboard configurations as the panes rotate. Use this gallery to see all **45** widgets in action.

|  ![Rotation 1](images/v1_0/hc_v1_dark_robinson_muf_rot1_asteroids_forecast_wx_alerts_ncdxf_de_info_satellite.png)  |  ![Rotation 2](images/v1_0/hc_v1_dark_robinson_muf_rot2_band_cond_ionosonde_santa_tracker_dx_weather_de_info_satellite.png)  |
|  ![Rotation 3](images/v1_0/hc_v1_dark_robinson_muf_rot3_aurora_graph_k-index_solar_flux_ncdxf_de_info_satellite.png)  |  ![Rotation 4](images/v1_0/hc_v1_dark_robinson_muf_rot4_aurora_lightning_solar_storm_de_weather_de_info_satellite.png)  |
|  ![Rotation 5](images/v1_0/hc_v1_dark_robinson_muf_rot5_contests_gimbal_tropics_dx_weather_de_info_satellite.png)  |  ![Rotation 6](images/v1_0/hc_v1_dark_robinson_muf_rot6_clock_aux_moon_system_info_ncdxf_de_info_satellite.png)  |
|  ![Rotation 7](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot1_aurora_graph_k-index_solar_flux_ncdxf_de_info_satellite.png)  |  ![Rotation 8](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot2_aurora_lightning_solar_storm_de_weather_de_info_satellite.png)  |
|  ![Rotation 9](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot3_contests_gimbal_tropics_dx_weather_de_info_satellite.png)  |  ![Rotation 10](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot4_clock_aux_moon_system_info_ncdxf_de_info_satellite.png)  |
| ![Rotation 11](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot5_callbook_meteor_scat_sunspots_de_weather_de_info_satellite.png) | ![Rotation 12](images/v1_0/hc_v1_midnight_azimuthal_aurora_rot6_countdown_marine_stopwatch_dx_weather_de_info_satellite.png) |
| ![Rotation 13](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot1_aurora_lightning_solar_storm_de_weather_de_info_satellite.png) | ![Rotation 14](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot2_contests_gimbal_tropics_dx_weather_de_info_satellite.png) |
| ![Rotation 15](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot3_clock_aux_moon_system_info_ncdxf_de_info_satellite.png) | ![Rotation 16](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot4_callbook_meteor_scat_sunspots_de_weather_de_info_satellite.png) |
| ![Rotation 17](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot5_countdown_marine_stopwatch_dx_weather_de_info_satellite.png) | ![Rotation 18](images/v1_0/hc_v1_paper_mercator_drap_isobars_rot6_drap_live_spots_tropo_cond_ncdxf_de_info_satellite.png) |
| ![Rotation 19](images/v1_0/hc_v1_glass_dual_az_clouds_rot1_contests_gimbal_tropics_dx_weather_de_info_satellite.png) | ![Rotation 20](images/v1_0/hc_v1_glass_dual_az_clouds_rot2_clock_aux_moon_system_info_ncdxf_de_info_satellite.png) |


---

## Space Weather

| Widget           | Description                                                                                                   | Data Source              |
| ---------------- | ------------------------------------------------------------------------------------------------------------- | ------------------------ |
| **Solar**        | Solar flux index (SFI), sunspot number (SSN), X-ray flux, proton flux, solar wind speed and density, Kp index | NOAA SWPC                |
| **SDO**          | Live solar imagery from the Solar Dynamics Observatory in selectable wavelengths; optional movie loop         | NASA SDO                 |
| **Aurora**       | Global aurora forecast map from the NOAA OVATION model                                                        | NOAA SWPC                |
| **Aurora Graph** | Time-series graph of geomagnetic Kp index and aurora activity                                                 | NOAA SWPC                |
| **DRAP**         | D-Region Absorption Prediction \u2014 HF absorption map showing where ionospheric absorption degrades propagation  | NOAA SWPC                |
| **Solar Storm**  | Current geomagnetic storm watch/warning/alert status                                                          | NOAA SWPC                |
| **History Flux** | Historical solar flux (SFI) graph over multiple days                                                          | NOAA SWPC                |
| **History KP**   | Historical Kp geomagnetic index graph                                                                         | NOAA SWPC                |
| **History SSN**  | Historical sunspot number graph                                                                               | NOAA SWPC                |
| **DST Index**    | Disturbance Storm Time index \u2014 a measure of geomagnetic storm severity                                        | NOAA SWPC (Kyoto)        |
| **Ionosonde**    | Ionospheric sounding data                                                                                     | Remote ionosonde network |

![Solar widget](images/widget-solar-flux.png)

![SDO widget](images/widget-sdo.png)

---

## Propagation

| Widget              | Description                                                                                 | Data Source        |
| ------------------- | ------------------------------------------------------------------------------------------- | ------------------ |
| **Band Conditions** | Color-coded HF band condition summary (160m\u201310m) by path type                               | NOAA SWPC derived  |
| **Live Spots**      | Real-time decoded signal spots from PSK Reporter or Reverse Beacon Network, plotted by band | PSK Reporter / RBN |
| **NCDXF**           | NCDXF/IBP international beacon schedule and current beacon on air                           | NCDXF              |

![Band Conditions widget](images/widget-band-conditions.png)

![Live Spots widget](images/widget-live-spots.png)

---

## DX / Spots

| Widget           | Description                                               | Data Source                       |
| ---------------- | --------------------------------------------------------- | --------------------------------- |
| **DX Cluster**   | Live DX spots from a telnet DX cluster or WSJT-X UDP feed          | Configured cluster host or WSJT-X |
| **Greyline DX**  | DXCC entities currently in the greyline with minutes to peak offset | Internal / Astronomical calculation |
| **DX Peditions** | Upcoming and active DXpeditions list                                | ng3k.com                          |
| **On The Air**   | Active POTA and SOTA activations worldwide                | POTA / SOTA APIs                  |
| **ADIF**         | Displays recent QSOs from a local ADIF log file           | Local file                        |
| **Callbook**     | Callsign lookup results (name, QTH, grid)                 | Callook / HamDB / QRZ             |
| **Watchlist**    | Monitor specific callsigns in the DX cluster stream       | DX Cluster (filtered)             |
| **Alerts**       | Triggered alerts based on watchlist or band activity      | Internal                          |

![DX Cluster widget](images/widget-dx-cluster.png)

![DX Peditions widget](images/widget-dx-peditions.png)

![On The Air (POTA/SOTA) widget](images/widget-on-the-air.png)

---

## Weather

| Widget         | Description                                                   | Data Source   |
| -------------- | ------------------------------------------------------------- | ------------- |
| **DE Weather** | Current weather conditions at your home location (DE)         | Open-Meteo    |
| **DX Weather** | Current weather conditions at the selected DX entity location | Open-Meteo    |
| **Forecast**   | Multi-day weather forecast for DE location                    | Open-Meteo    |
| **Hurricane**  | Active tropical storm / hurricane tracks                      | NHC / NOAA    |
| **Marine**     | Marine weather and sea state                                  | NOAA marine   |
| **Lightning**  | Real-time lightning strike activity map                       | Blitzortung   |
| **Tropo**      | Tropospheric ducting forecast index                           | APRS / custom |
| **Meteor**     | Meteor scatter activity / upcoming shower calendar            | IMO           |

![DE Weather widget](images/widget-dx-weather.png)

---

## Tracking

| Widget            | Description                                                                                                                                                   | Data Source                 |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------- |
| **Moon**          | Moon phase, rise/set times, azimuth/elevation, distance                                                                                                       | Astronomical calculation    |
| **Gimbal**        | Antenna rotator position display (requires Hamlib rotctld)                                                                                                    | Hamlib rotctld              |
| **EME Tool**      | Earth-Moon-Earth (moonbounce) window calculator                                                                                                               | Astronomical calculation    |
| **Santa Tracker** | Tracks Santa's position on Christmas Eve; activates on Dec 24 only (originally a hidden easter egg in the original HamClock \u2014 now a proper selectable widget) | Internal calculation        |
| **Asteroid**      | Next 5 close Earth approaches from the Minor Planet Center / JPL                                                                                              | JPL SBDB Close Approach API |

![Moon widget](images/widget-moon.png)

![Asteroid widget](images/widget-asteroids.png)

---

## Info / Utilities

| Widget           | Description                                                                                      | Data Source              |
| ---------------- | ------------------------------------------------------------------------------------------------ | ------------------------ |
| **DE Info**      | Your DE location details: callsign, grid, lat/lon, ITU zone, CQ zone, DXCC entity, bearing to DX | Internal config          |
| **DX Info**      | Details on the current DX target entity: name, prefix, zones, bearing/distance from DE           | Internal / DXCC database |
| **Clock Aux**    | Auxiliary digital clock. Click to cycle through timezone presets (UTC, EST, CST, MST, PST, CET, JST, AEST). Useful for showing local time alongside a DX-focused pane | Internal |
| **Countdown**    | Configurable countdown timer to a named event                                                    | Internal                 |
| **Stopwatch**    | Simple stopwatch                                                                                 | Internal                 |
| **Reminder**     | License expiry reminder and configurable date reminders                                          | Internal / FCC database  |
| **Repeater Dir** | Repeater directory lookup for your area (requires API key)                                       | RepeaterBook             |
| **Winlink**      | Winlink gateway listing for your area (requires access)                                          | Winlink API              |
| **Sys Info**     | System information: CPU, memory, network, uptime                                                 | Local OS                 |

![DE Info widget](images/widget-de-info.png)

![DX Info widget](images/widget-dx-info.png)

![Sys Info widget](images/widget-sys-info.png)

---

## Contests

| Widget       | Description                                | Data Source          |
| ------------ | ------------------------------------------ | -------------------- |
| **Contests** | Upcoming and active amateur radio contests | Contest calendar API |

![Contests widget](images/widget-contests.png)
