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

**To filter by band:**
Click a band label in the color legend at the bottom of the DX Cluster panel. Click it again to clear the filter.

**Tip:** The cluster shows mode badges (CW, SSB, FT8, FT4, RTTY) based on the spot frequency so you know what mode is being used even if the spotter didn't say.

---

## Live Spots (PSK Reporter / RBN / WSPR)

Live Spots shows you signals being heard around the world, updated continuously.

**It works immediately** — the default source is PSK Reporter, which requires no account.

**To filter by band:**
Setup → **Spotting** tab → Live Spots section → check or uncheck the bands you want to see.

**To switch sources:**
Same section — use the radio buttons to pick PSK Reporter, RBN (Reverse Beacon Network), or WSPR.

**Grid squares vs. callsigns:**
The "Use Callsign" toggle in the Spotting tab switches between showing grid locators and actual callsigns on the spots.

**Age limit:**
"Max Age" controls how many minutes old a spot can be before it disappears. Default is 30 minutes.

---

## On The Air (ONTA) — POTA & SOTA Activators

ONTA shows amateur radio operators who are currently activating parks (POTA) or summits (SOTA). Clicking a spot plots the great-circle path to them on the map.

**By default, it shows all active activations worldwide.** To narrow it down:

1. Click the gear icon on the ONTA panel, or go to Setup → **Watchlist** tab → ONTA section.
2. Choose **POTA**, **SOTA**, or **All**.
3. Set a **max distance in kilometers** to only show activators within range of your QTH. Leave it at 0 for no limit.

**Tip:** ONTA now shows all activators without a cap — scroll the list with your mouse wheel if there are more than fit on screen.

---

## Marine (Tides & Buoys)

The Marine widget shows tidal data and sea conditions from the nearest NOAA station to your QTH.

**When you first add it, Marine automatically finds and saves the nearest tide station and buoy.** You don't need to do anything.

**To switch to a specific station manually:**
Click the gear icon in the Marine panel → enter a NOAA tide station ID or NDBC buoy number → Save. The widget fetches new data immediately.

---

## Watchlist

The Watchlist monitors the DX Cluster for specific callsigns and alerts you when they appear.

1. Go to Setup → **Watchlist** tab.
2. Type a callsign → click **Add**. Repeat for each call you want to watch.
3. Optionally paste a comma-separated or space-separated list of callsigns all at once.

When a watched callsign appears in the cluster, the Watchlist panel highlights it. If audio is enabled, you'll also hear a voice alert.

**The list supports 30+ entries** — scroll with your mouse wheel when it gets long.

**Global alerts:** Watchlist notifications fire even if the Watchlist widget isn't currently visible in a pane.

---

## SDO (Solar Dynamics Observatory)

SDO shows near-real-time images of the sun captured by NASA's Solar Dynamics Observatory satellite.

Click the **gear icon** in the SDO panel to:
- Choose a **wavelength** (each shows different solar features):
  - 193 Å — hot plasma, active regions, flares
  - 171 Å — corona, quiet sun
  - 304 Å — chromosphere, prominences
  - 1600 Å — UV, sunspot regions
  - 1700 Å — photosphere, sunspot contrast
- Enable **Rotating Display** to cycle through wavelengths automatically.
- Enable **PFSS Overlay** to show magnetic field lines.
- Enable **Movie Mode** for an animated loop.

---

## Asteroid

The Asteroid widget tracks near-Earth objects from the JPL Close Approach database. It uses your QTH location to show each asteroid's current position in your sky (azimuth and elevation).

**No setup needed.** When you add it to a pane, it fetches data automatically. Scroll the list with your mouse wheel — there's no cap on how many asteroids are shown.

Click an asteroid in the list to select it; the polar plot updates to show its sky position.

---

## Satellite Tracking

1. Add the Satellite widget to a pane.
2. Click the **gear icon** in the panel → select a satellite from the list.
3. The map shows the satellite's ground track and footprint automatically.

**With a rotator (antenna controller):** If you have `rotctld` (Hamlib) set up (see below), enable **Auto-Track** in the rotator settings to automatically point your antenna at the satellite as it passes.

---

## Callbook Lookup (DX Info / DX Cluster)

When you click a callsign in the DX Cluster or DX Info panel, HamClock looks up that call in a callbook database.

**Free options (no account needed):** HamDB and Callook work automatically with no configuration.

**QRZ (enhanced data, requires subscription):**
Setup → **Services** tab → enter your QRZ username and password.

---

## RepeaterBook

Shows local repeaters from the RepeaterBook directory.

**Requires a free API key:**
1. Get a key at [repeaterbook.com](https://www.repeaterbook.com)
2. Setup → **Services** tab → enter the key in the RepeaterBook field.

---

## Winlink

Shows Winlink gateway status near your QTH.

**Requires an API key:**
Setup → **Services** tab → enter your Winlink API key.

---

## Rig Control (CAT — frequency/mode display)

Shows your rig's current frequency and mode, and optionally lets you tune from HamClock.

**Requires `rigctld` from Hamlib** running on your system or network:

```bash
# Example for a Yaesu FT-991A on /dev/ttyUSB0:
rigctld -m 1035 -r /dev/ttyUSB0 -s 38400
```

Then in HamClock: Setup → **Rig** tab → set Host (`localhost` if local) and Port (default `4532`).

Enable **Auto-Tune** to allow HamClock to change your rig's frequency when you click a DX Cluster spot.

---

## Rotator Control

Controls your antenna rotator so it tracks a selected satellite automatically.

**Requires `rotctld` from Hamlib** running on your system or network:

```bash
# Example for a Yaesu G-450A rotator:
rotctld -m 601 -r /dev/ttyUSB1
```

Then in HamClock: Setup → **Network** tab → Rotator section → set Host and Port (default `4533`).

Enable **Auto-Track** to automatically point your antenna at the selected satellite. Enable **Up-Over** if your rotator can handle passes through the zenith.

---

## Aux Clock

The Aux Clock widget shows a second timezone alongside your local time.

Click the **Aux Clock widget** itself to cycle through preset timezones (UTC, EST, CST, MST, PST, CET, JST, AEST). The label updates automatically.

To set a custom timezone: Setup → **Appearance** tab → Aux Clock section → enter the offset in hours and a label.

---

## Countdown Timer

Shows a countdown to a specific date and time — useful for contest start times, DXpedition windows, or satellite passes.

Click the **gear icon** in the Countdown panel to set:
- A label (e.g., "CQ WW SSB")
- Target date and time (UTC)
