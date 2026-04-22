# Data Sources & Network

HamClock-Next fetches data from a variety of public APIs and services. This page lists all data sources, their endpoints, update intervals, and which widgets use them.

All network requests are made by the HamClock-Next process itself. In browser (WASM) builds, requests are routed through a configured CORS proxy (`corsProxyUrl`).

---

## Space Weather (NOAA SWPC)

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| Planetary K-Index (3-hour) | `https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json` | 3 hours | Solar, History KP |
| Solar Flux Index (10cm) | `https://services.swpc.noaa.gov/products/summary/10cm-flux.json` | ~1 hour | Solar |
| Solar Cycle Prediction | `https://services.swpc.noaa.gov/json/solar-cycle/predicted-solar-cycle.json` | Daily | Solar |
| Solar Wind Plasma (5-min) | `https://services.swpc.noaa.gov/products/solar-wind/plasma-5-minute.json` | 5 min | Solar |
| Solar Wind Plasma (7-day) | `https://services.swpc.noaa.gov/products/solar-wind/plasma-7-day.json` | On demand | Solar (backfill) |
| Solar Wind Magnetometer (5-min) | `https://services.swpc.noaa.gov/products/solar-wind/mag-5-minute.json` | 5 min | Solar |
| Solar Wind Magnetometer (7-day) | `https://services.swpc.noaa.gov/products/solar-wind/mag-7-day.json` | On demand | Solar (backfill) |
| Kyoto DST Index | `https://services.swpc.noaa.gov/products/kyoto-dst.json` | 1 hour | DST Index |
| Aurora Forecast (OVATION) | `https://services.swpc.noaa.gov/json/ovation_aurora_latest.json` | 30 min | Aurora, Aurora Graph |
| DRAP Global Frequencies | `https://services.swpc.noaa.gov/text/drap_global_frequencies.txt` | 15 min | DRAP widget, DRAP overlay |
| GOES X-Ray (6-hour) | `https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json` | 1 min | Solar |
| GOES Proton Flux (6-hour) | `https://services.swpc.noaa.gov/json/goes/primary/integral-protons-6-hour.json` | 5 min | Solar |

---

## Solar Imagery (NASA SDO)

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| SDO AIA Images | `https://sdo.gsfc.nasa.gov/assets/img/latest/latest_256_{wavelength}.jpg` | 15 min | SDO |
| SDO AIA Movie | `https://sdo.gsfc.nasa.gov/assets/img/latest/` | On demand | SDO (movie mode) |

---

## Propagation

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| PSK Reporter spots | `https://retrieve.pskreporter.info/query?...` | 15 min | Live Spots, Heatmap overlay |
| RBN (Reverse Beacon Network) | `telnet://telnet.reversebeacon.net:7000` | Real-time | Live Spots (RBN mode), Heatmap overlay |
| WSPR spots | `https://db1.wspr.live/...` | 15 min | Heatmap overlay |
| DX Cluster (telnet) | Configured host:port | Real-time | DX Cluster |
| WSJT-X UDP feed | UDP port 2237 (configurable) | Real-time | DX Cluster (WSJT-X mode) |

---

## Callbook

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| Callook (free) | `https://callook.info/{callsign}/json` | On demand | Callbook widget |
| HamDB (free) | `http://api.hamdb.org/{callsign}/json/hamclock-next` | On demand | Callbook widget |
| QRZ (subscription) | QRZ XML API | On demand | Callbook widget |

---

## DX / Awards

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| DX Peditions list | `https://ng3k.com/misc/adxo.html` | 4 hours | DX Peditions |
| POTA activations | POTA API | 15 min | On The Air |
| SOTA activations | SOTA API | 15 min | On The Air |
| Contest calendar | Contest API | Daily | Contests |

---

## Weather

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| Current conditions | Open-Meteo | 30 min | DE Weather, DX Weather |
| Forecast | Open-Meteo | 1 hour | Forecast widget |
| Cloud cover overlay | NOAA NOMADS GFS GRIB2 (TCDC) | 6 hours | Map weather overlay |
| Hurricane tracks | NHC / NOAA | 6 hours | Hurricane widget |

---

## Astronomy & Tracking

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| Asteroid close approaches | `https://ssd-api.jpl.nasa.gov/cad.api` | Daily | Asteroid widget |
| Satellite TLE data | Celestrak / Space-Track | 1–7 days | Satellite tracking, Gimbal |
| Moon position | Calculated internally (no network) | — | Moon widget, EME Tool |

---

## Ionosphere / HF

| Data | Endpoint | Interval | Used By |
|------|----------|----------|---------|
| Ionosonde data | Remote ionosonde network | 15 min | Ionosonde widget |
| VOACAP predictions | VOACAP online API or local engine | On demand | VOACAP map overlays |

---

## Network Requirements Summary

HamClock-Next works best with a broadband internet connection. Most data sources update every 5–30 minutes; a connection faster than 1 Mbps is sufficient.

In **offline mode** (no internet), HamClock-Next continues to display:
- The world map (stored locally)
- Moon phase and astronomical calculations (computed locally)
- Local clock and DE information
- Any data already cached from the last successful fetch

All other widgets show a "No data" state until connectivity is restored.

---

## Privacy Notes

- HamClock-Next sends your callsign to callbook APIs (Callook, HamDB, QRZ) only when a callsign lookup is triggered.
- Your location (lat/lon) is sent to weather APIs to retrieve local conditions.
- No telemetry or usage data is collected by HamClock-Next itself.
