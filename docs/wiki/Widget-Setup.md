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

**Weather & Environment**
- DE Weather, DX Weather, Forecast, Hurricane, Lightning, Tropo, Meteor

**Tracking** *(uses your home location)*
- Moon, EME Tool, Greyline DX, Greyline Windows, Santa Tracker

**Info & Utilities**
- DE Info, DX Info, Stopwatch, Sys Info, ADIF Log, Contests, VOACAP DE-DX
- Solar Cycle 25 Tracker, DXCC Progress

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
- **Sidereal Mode**: Advanced users can enable Sidereal time display through the configuration menu.

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
   - **Windows**: voice alerts are not available on Windows at this time.
2. Restart HamClock-Next. If `flite` is found, voice alerts are active automatically.
3. To silence all voice alerts temporarily, use the global **audio mute** setting (Setup → **Audio** tab or REST API `/set_config?audio_mute=1`).

---

## Map Navigation

- **Zoom**: Use the **mouse wheel** to zoom in (up to 10x) and out.
- **Pan**: **Left-click and drag** the map while zoomed.
- **Reset**: **Double-right-click** anywhere on the map to reset zoom/pan.
- **Pins**: Spotting pins (DX Cluster, PSK) are only visible when the corresponding widget is active in a pane.
