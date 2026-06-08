# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A hardware music controller for macOS Apple Music. An Arduino Nano ESP32 with an ILI9341 320x240 TFT display and physical buttons/rotary encoder communicates over USB serial with a Python desktop agent that controls Apple Music via AppleScript.

## Architecture

**Two-component system connected via serial (115200 baud):**

1. **Desktop host** — macOS process that polls Apple Music state via AppleScript, sends display updates to the Arduino, and receives button/dial commands. Auto-discovers the "Nano ESP32" USB serial device.
   - **`controller-host/`** (Go, preferred) — uses `github.com/andybrewer/mack` for AppleScript and `go.bug.st/serial` for serial. Batches metadata into a single AppleScript call for efficiency.
   - **`desktop/monitor.py`** (Python, legacy) — uses `applescript` and `pyserial` packages.

2. **`Music_controller_arduino/Music_controller_arduino.ino`** — Arduino sketch for Nano ESP32. Drives the TFT display (track info, progress bar), reads 5 physical switches (A0–A4) and a rotary encoder (D2/D3), sends commands to the desktop agent.

**Serial protocol** — newline-delimited, 2-character command prefix + optional space + argument:
- Desktop → Arduino: `BA`/`AL`/`TR` (metadata), `TL`/`TP` (track length/position in 1/10s), `PL`/`PA` (play/pause state), `OP` (pipe-delimited option list)
- Arduino → Desktop: `PP` (play/pause), `SK` (skip), `FA` (favourite), `LL` (list playlists), `SY` (sync request), `JU` (jump ±seconds), `PL`/`SH` (play/shuffle playlist by name)

**Screen states:** `PLAYER` (now-playing with progress bar) and `OPTIONS` (playlist picker, up to 24 items in 2 columns).

## Hardware/Build

- **Board:** Arduino Nano ESP32 — must use "Pin numbering: by GPIO (legacy)" in Arduino IDE
- **Display:** ILI9341 via SPI, configured in `eTFT_User_Setup.h` (copy to TFT_eSPI library's `User_Setup.h`)
- **TFT_eSPI library** pin mapping: MISO=47, MOSI=38, SCLK=48, CS=18, DC=17, RST=10, BL=D10(GPIO21)
- **Do not upgrade Arduino ESP32 board package past 2.0.13** — TFT_eSPI is incompatible with newer versions
- Font headers (`SF-Pro-Display-Regular*.h`, `Chicago8*.h`, `Krungthep8.h`) are pre-generated anti-aliased font data for TFT_eSPI

## Desktop Agent Setup

**Go (preferred):**
```bash
cd controller-host
go build -o controller-host .
./controller-host
```

**Python (legacy):**
```bash
conda env create -f environment.yml
conda activate music-controller
python desktop/monitor.py
```

## CAD

`CAD/enclosure-row.scad` — OpenSCAD enclosure using the BOSL2 library. Generates an angled front panel with cutouts for 4 switches, a rotary encoder, and TFT screen, plus a screw-mounted base plate.
