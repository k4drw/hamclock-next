# Widget Setup Guide

This guide explains how to configure each widget in plain language. If you know amateur radio but don't consider yourself a computer expert, start here.

**Before anything else:** Open Setup (click the ⚙ gear icon in the top-left corner) and go to the **Identity** tab. Enter your callsign, grid locator, and location. Many widgets use your QTH automatically — if this isn't set, weather, distance filters, tides, and tracking widgets won't know where you are.

---

## Widgets That Work Automatically

These widgets fetch data on their own the moment you add them to a pane. No configuration needed.

**Space Weather**
- Solar, Aurora, Aurora Graph, DRAP, Solar Storm, DST Index, Ionosonde
- Band Conditions, NCDXF Beacons
- History Flux, History KP, History SSN
- K-Index Trend, SFI 30-Day Trend, NOAA Severity Scales, Space Weather Alerts

**Weather & Environment**
- DE Weather, DX Weather, Forecast, Hurricane, Lightning, Tropo, Meteor

**Tracking** *(uses your QTH location)*
- Moon, EME Tool, Greyline DX, Greyline Windows, Santa Tracker

**Info & Utilities**
- DE Info, DX Info, Stopwatch, Sys Info, ADIF Log, Contests, VOACAP DE-DX
- Solar Cycle 25 Tracker, DXCC Progress

**Timers** *(configure once inside the widget)*
- Countdown — click the gear icon in the panel to set a label and target time
- Reminder — click the gear icon to set reminder text and time

---

## DX Cluster

The DX Cluster connects to a global spotting network and shows you who's working what, right now.

**It works immediately** using a public cluster (dxusa.net) — just add it to a pane.

**To use your club's cluster or a preferred server:**
Setup → **Spotting** tab → DX Cluster section → enter the hostname and port number.

**To include spots from WSJT-X on your own computer:**
Setup → **Spotting** tab → enable **WSJT-X Mode**. HamClock listens for UDP packets on port 2237 (the WSJT-X default).

**Filtering and Performance:**
- **Band filter**: Click a band label in the color legend at the bottom of the DX Cluster panel. Click it again to clear the filter.
- **Duplicate Hiding**: (New) Go to Setup → **Spotting** tab and enable **Hide Duplicates**. Only the latest spot for any given callsign on a specific band will be shown, reducing clutter during busy events.

**Tip:** The cluster shows mode badges (CW, SSB, FT8, FT4, RTTY) based on the spot frequency.

---

## Live Spots (PSK Reporter / RBN / WSPR)

Live Spots shows you signals being heard around the world, updated continuously.

**It works immediately** — the default source is PSK Reporter.

**In-Widget Configuration:**
- Click a **band label** directly in the widget header to toggle that band on/off.
- Click the **Source** name (e.g., PSK) to cycle between PSK Reporter, RBN (Reverse Beacon Network), and WSPR.

**Configuration Screen Options:**
Setup → **Spotting** tab → Live Spots section:
- **Use Callsign**: Show actual callsigns instead of grid locators.
- **Max Age**: Controls how many minutes old a spot can be before it disappears.

---

## On The Air (ONTA) — POTA & SOTA Activators

ONTA shows operators activating parks (POTA) or summits (SOTA). Clicking a spot plots the path on the map.

- **Filters**: Click the gear icon on the ONTA panel to choose **POTA**, **SOTA**, or **All**.
- **Distance**: Set a maximum distance filter in the ONTA settings to only see activators within range of your QTH.
- **Legend**: Double-height mode shows a band color legend at the bottom for quick reference.

---

## World Clock

Display up to four specific timezones with labels.

1. Add **World Clock** to a pane.
2. Click the **gear icon** inside the widget.
3. For each of the 4 slots:
   - Enter a **City Label** (e.g., "London").
   - Set the **UTC Offset** (e.g., 0, -5, +1).
4. Save to see the new times.

---

## Big Clock

A full-pane clock for high visibility from across the room.

1. Add **Big Clock** to a pane.
2. Click the **gear icon** to configure:
   - **Mode**: Digital or Analog.
   - **Format**: 12-hour or 24-hour.
   - **UTC**: Lock to UTC or use your default local timezone.
   - **Sec/Date**: Toggle seconds and date display.
   - **Color**: Adjust the hue slider to match your theme.

---

## Marine (Tides & Buoys)

The Marine widget shows tidal data and sea conditions from the nearest NOAA station.

- **Auto-Lookup**: Click the **Find Closest** button in the Marine setup (gear icon) to automatically identify and select the nearest NOAA tide station and buoy.
- **Manual Entry**: Enter specific NOAA station or buoy IDs if you prefer a different location.

---

## SDO (Solar Dynamics Observatory)

SDO shows live solar imagery in various wavelengths.

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

**Rotator Control:** If using a rotator via `rotctld`, enable **Auto-Track** in the Rotator settings (Setup → Network) to point your antenna at the target automatically during a pass.

---

## Rig Control (Hamlib)

Shows your rig's frequency and mode, and allows tuning from HamClock.

- **Requirements**: Requires `rigctld` (Hamlib) running on your system.
- **Sync**: Enable **Auto-Tune** in Setup → Rig to automatically set your rig's frequency when you click a DX Cluster spot or band button.

---

## Aux Clock

Shows a second clock next to your local time.

- **Quick Cycle**: Click the widget itself to cycle through common timezones (UTC, EST, CST, MST, PST, CET, JST, AEST).
- **Custom Offset**: Set a custom offset and label in Setup → Appearance → Aux Clock.
- **Sidereal Mode**: Advanced users can enable Sidereal time display through the configuration menu.

---

## Map Navigation

- **Zoom**: Use the **mouse wheel** to zoom in (up to 10x) and out.
- **Pan**: **Left-click and drag** the map while zoomed.
- **Reset**: **Double-right-click** anywhere on the map to reset zoom/pan.
- **Pins**: Spotting pins (DX Cluster, PSK) are only visible when the corresponding widget is active in a pane.
