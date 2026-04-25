# Widget Setup Guide

This guide explains how to set up the main widgets in plain language. If you know amateur radio but do not consider yourself a computer expert, start here.

**Before anything else:** Open Setup (click the ⚙ gear icon in the top-left corner) and go to the **Identity** tab. Enter your callsign and [home station location](Glossary.md#qth). Many widgets use your [QTH](Glossary.md#qth) automatically, so if this is blank the weather, distance, tide, and tracking widgets will not know where you are.

---

## Widgets That Work Automatically

These widgets start working as soon as you add them to a pane. No extra setup is needed.

**Space Weather**
- Solar, Aurora, Aurora Graph, DRAP, Solar Storm, DST Index, Ionosonde
- Band Conditions, NCDXF Beacons
- History Flux, History KP, History SSN
- K-Index Trend, SFI 30-Day Trend, NOAA Severity Scales, Space Weather Alerts
- **K-Index Alert Threshold**: In the K-Index Alert widget, you can set a "trigger level." HamClock will only notify you (and the widget will only glow) when solar activity goes above your chosen number.

**Weather & Environment**
- DE Weather, DX Weather, Forecast, Hurricane, Lightning, Tropo, Meteor

**Tracking** *(uses your home location)*
- Moon, EME Tool, Greyline DX, Greyline Windows, Santa Tracker

**Award Tracking** *(requires a loaded ADIF log)*
- **WAS Progress**: Tracks your US State contacts and shows your progress on a map.
- **WAC Radar**: A 6-segment pie chart that shows which continents you have worked and confirmed.
- **Zone Heatmap**: A clickable grid that tracks your progress through CQ and ITU zones.
- **LoTW Auto-Sync**: Automatically sends your contacts to the ARRL "Logbook of The World" and shows your live confirmation count.

**Info & Utilities**
- DE Info, DX Info, Stopwatch, Sys Info, ADIF Log, Contests, VOACAP DE-DX
- Solar Cycle 25 Tracker, DXCC Progress
- **Frequency Cursor**: On graphs like the VOACAP DE-DX matrix, a white horizontal line shows exactly where your radio is currently tuned.

**Timers** *(configure once inside the widget)*
- Countdown - click the gear icon in the panel to set a label and target time
- Reminder - click the gear icon to set reminder text and time

---

## Band Conditions

Shows a color-coded summary of HF band conditions from 160m to 10m. It works automatically - no configuration needed.

**Expanded detail view:**
Click the pane's expand arrow or double-click the pane title bar to enlarge the Band Conditions tile. The expanded view shows more detail about why each band is rated the way it is.

---

## DX Cluster

The DX Cluster connects to a global spotting network and shows you who's working what, right now. See [DX Cluster](Glossary.md#dx--dx-cluster--spot) if the term is new.

**It works immediately** using a public cluster (`dxusa.net`) - just add it to a pane.

**To use your club's cluster or a preferred server:**
Setup → **Spotting** tab → DX Cluster section → enter the hostname and port number.

**To include spots from [WSJT-X](Glossary.md#wsjt-x) on your own computer:**
Setup → **Spotting** tab → enable **WSJT-X Mode**. HamClock listens for UDP packets on port 2237 (the WSJT-X default).

**Filtering and performance:**
- **Band filter**: Click a band label in the color legend at the bottom of the DX Cluster panel. Click it again to clear the filter.
- **Duplicate hiding**: Go to Setup → **Spotting** tab and enable **Hide Duplicates**. Only the latest spot for each callsign on a given band will be shown.
- **Max Age**: In Setup → **Spotting** tab, pick how old a spot is allowed to be before it drops off the list — **10, 20, 40, or 60 minutes**. Shorter windows keep the view focused on "right now" activity; longer windows are useful on quiet bands where spots are infrequent. The default is 20 minutes.

**Tip:** The cluster shows mode badges (CW, SSB, FT8, FT4, RTTY) based on the spot frequency.

**DXCC Award Tracking (N / B / W badges):**
If you have an ADIF log loaded (see the ADIF widget), each cluster spot is tagged:
- **N** - you have never worked that DXCC entity
- **B** - you have worked that entity before, but not on this band
- **W** - you have worked it on this band but it is not yet confirmed

No badge means you already have a confirmed QSO with that entity on this band.

---

## Live Spots (PSK Reporter / RBN / WSPR)

Live Spots shows real-time decoded signal spots from [PSK Reporter](Glossary.md#psk-reporter--rbn--wspr), [WSPR](Glossary.md#psk-reporter--rbn--wspr), or the [Reverse Beacon Network](Glossary.md#psk-reporter--rbn--wspr), displayed per band and plotted on the map.

**No login required.** PSK Reporter is free. HamClock-Next queries it using the callsign you set in Setup.

**Opening the configuration overlay:**
Click the **Counts** label at the bottom of the Live Spots widget. This opens an in-widget setup screen where you can change all options below.

**Source:**
Choose **PSK Reporter**, **WSPR**, or **RBN** (Reverse Beacon Network).

**Mode - "of DE" vs "by DE":**
This is the most important setting and the most common source of confusion about zero spots.

- **of DE** (default): *Who heard me?* - Queries PSK Reporter for stations that decoded **your** transmissions. Results are zero if you have not been on the air recently, or if nobody decoded you during the selected time window.
- **by DE**: *What did I hear?* - Queries for signals decoded at your location. This shows activity near you even when you are not transmitting.

**Filter (by DE mode only):**
When in *by DE* mode, choose whether to filter by **Callsign** or **Grid**. Grid mode can show a broader picture of activity near you.

**Band toggles:**
Click any band cell in the widget to toggle it on (colored) or off (dark) for map plotting. A band with a colored background is active; spots on that band appear as pins on the map.

**Max Age:**
Controls the time window in minutes. Spots older than this value are excluded. Default is 30 minutes; increase it if you're seeing very few spots.

---

## On The Air (ONTA) — POTA & SOTA Activators

ONTA shows operators activating parks ([POTA](Glossary.md#pota--sota)) or summits ([SOTA](Glossary.md#pota--sota)). Clicking a spot plots the path on the map.

- **Filters**: Click the gear icon on the ONTA panel to choose **POTA**, **SOTA**, or **All**.
- **Distance**: Set a maximum distance filter in the ONTA settings to only see activators within range of your home location.
- **Legend**: Double-height mode shows a band color legend at the bottom for quick reference.

---

## DX Peditions

Shows upcoming and currently active DX expeditions — special operations by stations traveling to rare or desirable locations.

**Automatic setup:** No configuration needed. Just add it to a pane.

**Click to set DX location:** Click any row in the DX Peditions widget to instantly set that expedition's location as your DX target. The DX Info panel updates immediately, and propagation overlays (if active) recalculate for that path. This is faster than using the Setup screen for quick DX target switching.

---

## DX Info

Shows details about the station you are currently watching or have selected on the map.

**Direct Callsign Entry:**
Click anywhere on the DX Info widget to open a typing box. You can enter any callsign, and HamClock will immediately look up that station's location, distance, and local time. This is the fastest way to check a path for a station you just heard on the air.

---

## Watchlist & Alerts

The Watchlist lets you monitor specific callsigns in the [DX cluster](Glossary.md#dx--dx-cluster--spot) stream and get notified when they appear.

**Setting it up:**
1. Open Setup → **Spotting** tab → Watchlist section.
2. Enter the callsigns you want to watch, separated by commas or spaces (e.g., `VK2TT, JA1ABC, VP9GE`). You can paste up to 256 characters at once.
3. Save.

**What happens when a watched callsign is spotted:**
- An on-screen alert fires in the Alerts tile (if you have it in a pane).
- A voice alert plays if `flite` is installed (see [Voice Alerts](#voice-alerts-text-to-speech) below).

**You do not need the Watchlist tile visible for alerts to fire.** Monitoring runs in the background at all times, even if you do not have the Watchlist widget on screen.

**Paste in bulk:** You can paste a full list of callsigns directly into the Watchlist setup field — commas, spaces, or a mix both work as separators.

---

## Greyline DX

Shows DXCC entities whose sunrise or sunset is happening now. These are often the best times for long-distance contacts.

Each row shows a countdown in minutes to the gray line peak. As the window gets close, the row changes color to draw your attention. Entities near peak are the highest priority for a contact — the window is only about 30 minutes wide.

The tile works automatically as soon as you add it to a pane. It uses your home location and astronomical calculations, so no extra setup is needed.

---

## World Clock

Display up to four time zones with labels.

1. Add **World Clock** to a pane.
2. Click the **gear icon** inside the widget.
3. For each of the 4 slots:
   - Enter a **City Label** (e.g., "London").
   - Set the **UTC Offset** (e.g., 0, -5, +1).
4. Save to see the new times.

---

## Big Clock

A full-pane clock that is easy to read from across the room.

1. Add **Big Clock** to a pane.
2. Click the **gear icon** to configure:
   - **Mode**: Digital or Analog.
   - **Format**: 12-hour or 24-hour.
   - **UTC**: Lock to UTC or use your default local time zone.
   - **Sec/Date**: Toggle seconds and date display.
   - **Color**: Adjust the hue slider to match your theme.

---

## Marine (Tides & Buoys)

The Marine widget shows tidal data and sea conditions from the nearest NOAA station.

- **Auto-Lookup**: Click the **Find Closest** button in the Marine setup (gear icon) to automatically identify and select the nearest NOAA tide station and buoy.
- **Manual Entry**: Enter specific NOAA station or buoy IDs if you prefer a different location.

---

## SDO (Solar Dynamics Observatory)

SDO shows live solar imagery in various wavelengths. See [SDO](Glossary.md#sdo-solar-dynamics-observatory) if the acronym is new.

Click the **gear icon** in the SDO panel to:
- Choose a **wavelength** (0193, 0171, 0304, 1600, 1700, HMIB, HMIIC).
- Enable **Rotating Display** to cycle wavelengths automatically.
- Enable **PFSS Overlay** for magnetic field lines.
- Enable **Movie Mode** for a time-lapse loop.

---

## Satellite Tracking

1. Add the Satellite widget to a pane.
2. Click the **gear icon** in the panel → select a satellite from the list.
3. The map shows the ground track and footprint automatically.

**Rotator Control:** If using a rotator via [rotctld](Glossary.md#rotctld--rigctld), enable **Auto-Track** in the Rotator settings (Setup → Network) to point your antenna at the target automatically during a pass.

---

## Rig Control (Hamlib)

Shows your rig's frequency and mode, and allows tuning from HamClock.

- **Requirements**: Requires [`rigctld`](Glossary.md#rotctld--rigctld) (Hamlib) running on your system.
- **Sync**: Enable **Auto-Tune** in Setup → Rig to automatically set your rig's frequency when you click a DX Cluster spot or band button.

---

## Aux Clock

Shows a second clock next to your local time.

- **Quick Cycle**: Click the widget itself to cycle through common time zones (UTC, EST, CST, MST, PST, CET, JST, AEST).
- **Custom Offset**: Set a custom offset and label in Setup → Appearance → Aux Clock.
- **Sidereal Mode**: Switches the Aux Clock from a regular zone clock to [sidereal time](Glossary.md#sidereal-time) — the "clock of the stars" used in astronomy. It is useful for radio astronomy, meteor-scatter timing, and planning EME (moon-bounce) sessions against a fixed star background. Set `auxClockStarMode` in the config to enable it; leave at `0` for a normal zone clock.

---

## RSS News Ticker

A scrolling news headline strip can appear along the bottom edge of the screen, pulling headlines from any RSS feed (amateur radio news sites, space weather alerts, etc.).

**To enable it:**
- REST API: `GET /set_rss?enabled=1`
- Or add `"rssEnabled": true` to your config file manually.

**To choose a feed:** Set `"rssUrl"` in your config file to the URL of any RSS feed. The default feed covers amateur radio news headlines.

**To disable it:** `GET /set_rss?enabled=0` or set `"rssEnabled": false`.

The ticker is silent — it only shows text. If the feed can't be reached, the strip disappears rather than showing an error.

---

## Alarms

HamClock-Next can play an audio alert at a time you choose. It is useful as a morning reminder to check into a net or to mark the start of a contest.

**Daily alarm** — fires every day at the same time:
1. Open Setup → **Timers** tab.
2. Set the hour and minute.
3. Choose **UTC** or **local time**.
4. Enable the alarm and save.

**One-time alarm** — fires once at a specific date and time, then disables itself:
1. Open Setup → **Timers** tab.
2. Enable the one-time alarm and enter the target date/time.
3. Save.

If no sound plays when the alarm fires, check that your system audio is working and that HamClock-Next was not built with `--no-audio`. See [Building from Source](Building-from-Source.md) if you built it yourself.

---

## Voice Alerts (Text-to-Speech)

HamClock-Next can speak alerts out loud using your computer's speakers.

Spoken alerts fire for:
- **Watchlist hits** - a callsign you are monitoring appears in the DX cluster
- **Solar flares** - a significant X-ray event is detected
- **Countdown reaching zero** - your Countdown timer expires
- **Calendar reminders** - a Calendar event you set is due

**To enable voice alerts:**
1. Install `flite` on your system:
   - **Linux / Raspberry Pi**: `sudo apt install flite`
   - **macOS**: `brew install flite`
   - **Windows**: voice alerts are handled natively by Windows.
2. Restart HamClock-Next. If `flite` is found (on Linux/Mac), voice alerts are active automatically.

**Volume Control:**
You can adjust the loudness of all voice alerts and chimes using the **Volume** slider in Setup → **Identity** tab. 

3. To silence all voice alerts temporarily, use the global **audio mute** setting (Setup → **Audio** tab or REST API `/set_config?audio_mute=1`).

---

## ADIF Log

The ADIF Log tile shows a scrollable list of your recent QSOs and a summary of your log (total contacts, bands worked, entities confirmed).

It works as soon as you load a log file. There are two ways to do that:

**Option 1 — REST API (recommended for automation):**
```
POST /set_adif
```
Send your ADIF file as the request body. For example, using curl:
```bash
curl -X POST --data-binary @yourlog.adi http://localhost:8080/set_adif
```

**Option 2 — Direct file placement:**
Copy your ADIF file to `~/.config/hamclock-next/log.adi` (Linux/Raspberry Pi) or the equivalent config folder on your platform, then restart the app.

Once a log is loaded, **DXCC award tracking** activates for the DX Cluster tile as well — spots for new entities or unconfirmed bands will show N/B/W badges automatically. See [DX Cluster](#dx-cluster) above.

**Filtering inside the tile:** Click the band or mode label in the tile header to filter the log view to that band or mode. Click again to cycle through options.

---

## Winlink

The Winlink tile shows nearby [Winlink](Glossary.md#winlink) gateway stations — useful for checking which digital relay nodes are reachable from your location.

**Requirements:** You need a free Winlink API key. Register at `winlink.org` and note your access key.

**Setting it up:**
1. Open Setup → **Services** tab.
2. Enter your Winlink API key in the **Winlink Key** field.
3. Save.

The Winlink tile now becomes available in the widget picker. Once added to a pane, it queries for gateways near your home location automatically.

---

## Map Navigation

- **Zoom**: Use the **mouse wheel** to zoom in (up to 10x) and out.
- **Pan**: **Left-click and drag** the map while zoomed.
- **Reset**: **Double-right-click** anywhere on the map to reset zoom/pan.
- **Pins**: Spotting pins (DX Cluster, PSK) are only visible when the corresponding widget is active in a pane.
