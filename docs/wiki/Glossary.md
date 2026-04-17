# Glossary of Technical Terms

This page explains technical terms used elsewhere in the HamClock-Next documentation. If you are comfortable with amateur radio but less familiar with computer software, this is a good reference.

---

## AOS / LOS (Acquisition of Signal / Loss of Signal)

The moments when a satellite pass begins (**AOS**) and ends (**LOS**) as seen from your location. HamClock-Next displays AOS and LOS times in the Satellite widget so you know exactly when to point your antenna and when the pass is over.

---

## Configuration File

The file where HamClock-Next stores all of your settings. It uses [JSON](#json) format and is a plain text file you can open with any text editor. You normally don't need to edit it directly — the Setup screen (gear icon ⚙) handles the common settings for you.

File locations:
- **Linux / Raspberry Pi:** `~/.config/hamclock-next/config.json`
- **macOS:** `~/Library/Application Support/hamclock-next/config.json`
- **Windows:** `%APPDATA%\hamclock-next\config.json`

---

## DE

Ham shorthand for **your own station** or **home station**. HamClock-Next uses DE to label your location on the screen, map, and related widgets.

---

## DX / DX Cluster / Spot

**DX** is amateur radio shorthand for a distant or rare station worth contacting.

A **DX Cluster** is a worldwide network of servers where operators report ("spot") active DX stations in real time. Each report is called a **spot** and includes the callsign, frequency, and time of the observation. HamClock-Next can connect to a DX Cluster via [Telnet](#telnet) or receive spots from WSJT-X to show live activity on the map and in the DX Cluster widget.

---

## DXCC

**DX Century Club** — an award from the American Radio Relay League (ARRL) recognizing confirmed contacts with 100 or more different countries and territories (called "entities"). HamClock-Next can display DXCC entity information for spotted callsigns.

---

## QTH

Your operating location or station location. In plain language, it means "where you are." HamClock-Next uses your QTH to center maps, calculate bearings, and fill in weather and propagation widgets.

---

## CORS Proxy

When HamClock-Next runs inside a web browser, the browser may block direct requests to some websites. A CORS proxy is a small relay program that forwards those requests for the browser. Think of it as a mail forwarder.

If you are running the pre-packaged browser build, this is already handled. You only need to configure `corsProxyUrl` if you are hosting your own instance.

---

## FT8

A digital weak-signal radio mode used for amateur HF communication. FT8 exchanges happen in 15-second intervals and can be decoded at very low signal levels, making it popular for propagation testing and DX contacts. HamClock-Next can receive FT8 spots from **WSJT-X** and display them on the map.

---

## Hamlib

A free, open-source package that lets computer programs talk to amateur radio equipment. HamClock-Next uses Hamlib to communicate with your transceiver (rig) and antenna rotator.

Hamlib works through two small helper programs that run in the background:
- `rigctld` — talks to your radio
- `rotctld` — talks to your rotator

See also: [rotctld / rigctld](#rotctld--rigctld).

---

## KMSDRM

The Linux graphics path HamClock-Next can use to draw directly to the screen without a desktop window system. This is mainly relevant on Raspberry Pi systems running in console mode.

---

## SDL2

The cross-platform graphics library HamClock-Next uses to draw its interface. SDL2 lets the app run on Linux, macOS, Windows, and in a browser build.

---

## Hostname

The human-readable name of a computer on a network (e.g., `dxusa.net` or `raspberrypi.local`). You enter a hostname — or an IP address — when configuring HamClock-Next to connect to an external service such as a DX Cluster server, `rigctld`, or `rotctld`. A hostname is always paired with a **[port number](#port--port-number)** to form a complete network address.

---

## I²C {#i2c}

A short-distance electrical connection standard used between a Raspberry Pi and a sensor (such as a BME280 temperature/humidity/pressure sensor). The sensor connects directly to the Pi's GPIO header with just two signal wires. You won't encounter this unless you are adding a hardware sensor.

---

## JSON

**J**ava**S**cript **O**bject **N**otation. A simple, human-readable text format for storing structured data. HamClock-Next uses it for its configuration file. It looks like this:

```json
{
  "callsign": "W1AW",
  "grid": "FN31",
  "useMetric": true
}
```

You can open and edit a JSON file with any plain text editor. The key rules are:
- Text values go inside `"double quotes"`
- Numbers and true/false values do **not** use quotes
- Each setting is a `"name": value` pair
- Items are separated by commas

---

## Maidenhead Grid Square

A global location system used by amateur radio operators. The world is divided into a grid of rectangles identified by two letters and two numbers (e.g., `FN31`). Larger squares (4 characters) are accurate to about 100 km; smaller squares (6 characters) narrow it to roughly 5 km. HamClock-Next uses your grid square to calculate beam headings, distances, and propagation paths.

---

## MUF (Maximum Usable Frequency)

The highest radio frequency that can make it through the ionosphere for a given path. Signals above the MUF pass through the ionosphere and are lost to space. HamClock-Next's propagation overlay can display the MUF so you can quickly see which bands are open for a given path.

---

## Port / Port Number

A number from 1 to 65535 that identifies a specific service on a networked computer. Think of a [hostname](#hostname) as a building's street address and the port number as the apartment number inside. For example, most DX Cluster servers listen on port `7300`. You enter both hostname and port in HamClock-Next when setting up network connections.

---

## PSK Reporter / RBN / WSPR

Three real-time networks that collect automatically decoded radio signals and share them publicly:

- **PSK Reporter** — reports digital-mode signals (FT8, FT4, PSK31, etc.)
- **RBN (Reverse Beacon Network)** — reports CW (Morse code) beacon transmissions
- **WSPR** (Weak Signal Propagation Reporter) — a dedicated weak-signal network used to map propagation

HamClock-Next can pull spots from these networks to populate the DX Cluster display and map overlays.

---

## POTA / SOTA

**POTA** is *Parks On The Air* and **SOTA** is *Summits On The Air*. These are amateur radio programs for portable operating from parks or summits. HamClock-Next uses them in the On The Air widget.

---

## REST API

A way for other programs — such as logging software, home automation systems, or custom scripts — to remote-control HamClock-Next over a network connection. Commands are sent as ordinary web addresses (URLs), and HamClock-Next responds with data.

For example, a logging program might tell HamClock-Next to display a new DX target after you log a QSO. You do not need to use or understand the REST API to use HamClock-Next normally.

See the [REST API reference](REST-API.md) if you want to build integrations.

---

## rotctld / rigctld

Two small helper programs that come with [Hamlib](#hamlib). They run as background services and act as a relay between HamClock-Next and your hardware:

- **`rigctld`** — connects to a transceiver (via USB, serial, or network)
- **`rotctld`** — connects to an antenna rotator controller

Once running, you enter the program's hostname and port number in HamClock-Next's Setup screen to connect. See the [Widget Setup Guide](Widget-Setup.md) for step-by-step instructions.

---

## RSS (Really Simple Syndication)

A standard format for publishing regularly updated content — such as news headlines or alerts — as a machine-readable feed. Programs subscribe to an RSS feed URL and receive new items automatically as they are published, without having to visit the website. HamClock-Next's News widget fetches headlines from RSS feeds (amateur radio news sites, space weather alerts, etc.) and displays them in a scrolling ticker.

---

## SDO (Solar Dynamics Observatory)

A NASA solar observatory that provides the live solar images shown in the SDO widget.

---

## Telnet

An old-fashioned text-based connection method, like a telephone call between two computers. HamClock-Next uses telnet to connect to [DX Cluster](#dx--dx-cluster--spot) servers (such as `dxusa.net`) to receive live spot feeds.

You configure the hostname and port number in Setup → DX Cluster. You do not need to understand telnet to use it — HamClock-Next handles the connection automatically.

---

## TLE (Two-Line Elements)

A compact, standardized text format for describing a satellite's orbit. Each satellite entry is exactly two lines of numbers encoding its altitude, inclination, and position. HamClock-Next downloads current TLE data from online sources (such as CelesTrak) to accurately compute satellite positions, pass times, and ground tracks.

---

## UDP Port

A numbered "mail slot" on your computer that programs use to send and receive data over a network. When WSJT-X is configured to broadcast spots, it sends them to a specific UDP port number (default: 2237). HamClock-Next listens on that port and picks up the spots.

If you change the port in WSJT-X, update the matching number in HamClock-Next's Setup → DX Cluster → WSJT-X port.

---

## VOACAP

**Voice of America Coverage Analysis Program** — a propagation prediction model originally developed for shortwave broadcasting. It estimates whether a radio signal on a given frequency is likely to travel between two points, based on time of day, season, solar activity, and antenna characteristics. HamClock-Next uses VOACAP data to draw its propagation overlay, showing which bands are likely open to different parts of the world from your location.

---

## WebAssembly (WASM)

A technology that lets programs written for desktop operating systems run inside a web browser, with no installation required. The HamClock-Next browser version uses WebAssembly. It behaves like the desktop version — all widgets, overlays, and configuration work the same way.

You may see "WASM" used as a shorthand for WebAssembly in technical discussions and release notes.

---

## WSJT-X

A popular digital-mode program used by amateur radio operators. HamClock-Next can use WSJT-X as a source of DX Cluster spots.
