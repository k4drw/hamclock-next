HAMCLOCK-NEXT DATA SOURCE REGISTRY - DO NOT EDIT MANUALLY - MANAGED BY MCP

# HamClock Data Source Registry

This document serves as the comprehensive registry of all external data sources utilized by HamClock-Next, categorized by their current integration status and future potential. It is intended to be the primary knowledge base for the MCP regarding data feeds.

## 1. Active Core Services

These services are currently integrated and actively used in HamClock-Next.

| Service Name          | Endpoint URL / Protocol                                           | Data Format          | Update Frequency | Technical Implementation Note                                     |
| :-------------------- | :---------------------------------------------------------------- | :------------------- | :--------------- | :---------------------------------------------------------------- |
| NOAA Space Weather    | https://services.swpc.noaa.gov/text/wwv.txt                       | Text (Regex parsing) | 15 mins          | Primary source for SFI, A-Index, K-Index.                       |
| Satellite Elements (TLEs) | https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle | TLE (3-line text)    | Daily            | Parsed by 'SatelliteManager'; fed into 'libpredict'.            |
| Live Band Activity    | https://pskreporter.info/cgi-bin/psk-query.pl                     | XML / ADIF           | Real-time        | Used for map spots.                                             |
| Solar Imagery         | https://sdo.gsfc.nasa.gov/assets/img/latest/                      | JPG (1024x1024)      | Real-time        | Requires 'SDL2_image' to render.                                |
| D-Region Absorption (DRAP) | https://services.swpc.noaa.gov/images/animations/d-rap/global/d-rap_global.png | PNG                  | 15 mins          |                                                                 |

## 2. Planned HamClock Parity

These services were present in the original HamClock and are slated for porting and integration into HamClock-Next.

| Service Name          | Endpoint URL / Protocol                                           | Data Format        | Update Frequency | Technical Implementation Note                             |
| :-------------------- | :---------------------------------------------------------------- | :----------------- | :--------------- | :-------------------------------------------------------- |
| DX Cluster            | Telnet (TCP Port 7300/7373); Example: dxc.nc7j.com                | Text Stream        | Real-time        | Requires persistent TCP socket and stream parsing.        |
| Contest Calendar      | https://www.contestcalendar.com/calendar.rss                      | RSS/XML            | Daily            | Needs XML parser.                                         |
| On The Air (POTA)     | https://api.pota.app/spot/activations                             | JSON               | 1 min            | REST API; shows active parks.                             |
| On The Air (SOTA)     | https://api-db.sota.org.uk/                                       | JSON               | 1 min            | Requires user account/auth token.                         |
| VOACAP (Band Conditions) | (Complex - typically via voacap.com/api/prediction)             | HTML/JSON          | On demand        | Often easier to calculate locally or scrape summary tables. |

## 3. Future Expansion & Research

This section outlines potential data sources for future features and enhancements not yet formally on the roadmap.

| Service Name          | Endpoint URL / Protocol                                           | Data Format        | Update Frequency | Technical Implementation Note                                     |
| :-------------------- | :---------------------------------------------------------------- | :----------------- | :--------------- | :---------------------------------------------------------------- |
| RepeaterBook (Local Repeaters) | https://www.repeaterbook.com/api/                           | JSON               | Monthly          | Great for 'Travel Mode' to find local machines based on GPS Lat/Lon. |
| APRS-IS (Position Reporting) | TCP (port 14580)                                                | Text (APRS packets)| Real-time        | Could show moving stations/objects on the map.                  |
| SatNOGS (Satellite Status) | https://db.satnogs.org/api/                                     | JSON               | Hourly           | Check if a satellite is actually alive/transmitting before trying to work it. |
| Reverse Beacon Network (RBN) | Telnet (telnet.reversebeacon.net 7000)                          | Text Stream        | Real-time        | Best source for 'Where is my signal being heard right now?'.    |
| QRZ.com (Callsign Detail) | https://xmldata.qrz.com/xml/current/                            | XML                | On demand        | Requires paid subscription XML key.                             |
| HamQSL (Solar Backup) | https://www.hamqsl.com/solarxml.php                               | XML                | 15 mins          | Excellent backup if NOAA SWPC changes their text format.        |