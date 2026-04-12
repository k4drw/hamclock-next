# Glossary of Technical Terms

This page explains technical terms used elsewhere in the HamClock-Next documentation. If you are comfortable with amateur radio but less familiar with computer software, this is a good reference.

---

## Configuration File

The file where HamClock-Next stores all of your settings. It uses [JSON](#json) format and is a plain text file you can open with any text editor. You normally don't need to edit it directly — the Setup screen (gear icon ⚙) handles the common settings for you.

File locations:
- **Linux / Raspberry Pi:** `~/.config/hamclock-next/config.json`
- **macOS:** `~/Library/Application Support/hamclock-next/config.json`
- **Windows:** `%APPDATA%\hamclock-next\config.json`

---

## CORS Proxy

When HamClock-Next runs inside a web browser, the browser's security rules prevent it from fetching data directly from the internet. A CORS proxy is a relay program on the same server that forwards those requests on behalf of the browser. Think of it as a mail forwarder.

If you are running the pre-packaged browser build, this is already handled. You only need to configure `corsProxyUrl` if you are hosting your own instance.

---

## Hamlib

A free, open-source software package that lets computer programs talk to amateur radio equipment. HamClock-Next uses Hamlib to communicate with your transceiver (rig) and antenna rotator.

Hamlib works through two small helper programs that run in the background:
- `rigctld` — talks to your radio
- `rotctld` — talks to your rotator

See also: [rotctld / rigctld](#rotctld--rigctld).

---

## I²C

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

## Telnet

An old-fashioned text-based connection method, like a telephone call between two computers. HamClock-Next uses telnet to connect to DX cluster servers (such as `dxusa.net`) to receive live spot feeds.

You configure the hostname and port number in Setup → DX Cluster. You do not need to understand telnet to use it — HamClock-Next handles the connection automatically.

---

## UDP Port

A numbered "mail slot" on your computer that programs use to send and receive data over a network. When WSJT-X is configured to broadcast spots, it sends them to a specific UDP port number (default: 2237). HamClock-Next listens on that port and picks up the spots.

If you change the port in WSJT-X, update the matching number in HamClock-Next's Setup → DX Cluster → WSJT-X port.

---

## WebAssembly (WASM)

A technology that allows programs written for desktop operating systems to run inside a web browser, with no installation required. The HamClock-Next browser version uses WebAssembly. It behaves identically to the desktop version — all widgets, overlays, and configuration work the same way.

You may see "WASM" used as a shorthand for WebAssembly in technical discussions and release notes.
