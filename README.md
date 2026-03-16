# Music Controller

A physical desktop controller for Apple Music on macOS. An Arduino Nano ESP32 with a colour TFT display and tactile controls sits on your desk, showing the currently playing track and giving you hardware buttons for playback control — no need to switch windows or reach for the keyboard.

## What it does

The controller displays the current artist, album, and track name on a 320x240 ILI9341 TFT screen with a progress bar showing playback position. Five buttons and a rotary encoder provide:

- **Play/Pause**
- **Skip track**
- **Favourite** the current track
- **Browse playlists** — scroll through your Apple Music playlists on the display and select one to play or shuffle
- **Seek** — twist the rotary encoder to jump forward or back within a track

A host program runs on your Mac and bridges the Arduino to Apple Music via AppleScript. It auto-detects the controller over USB, sends track metadata and timing to the display, and translates button presses into Music app commands. The display stays in sync even if you control playback from elsewhere — the host polls for changes every 10 seconds.

## Components

| Directory | Description |
|---|---|
| `controller-host/` | Go host program (preferred) — communicates with the Arduino over serial and controls Apple Music via AppleScript |
| `desktop/` | Legacy Python host (`monitor.py`) — same functionality, requires a conda environment |
| `Music_controller_arduino/` | Arduino sketch for the Nano ESP32 — drives the TFT display, reads buttons and rotary encoder |
| `CAD/` | OpenSCAD enclosure design with SVG button icons |

## Hardware

- **Arduino Nano ESP32**
- **ILI9341 320x240 SPI TFT display**
- **5 momentary push buttons** (connected to A0–A4)
- **Rotary encoder** (connected to D2/D3)
- 3D-printed enclosure (see `CAD/enclosure-row.scad`, requires the [BOSL2](https://github.com/BelfrySCAD/BOSL2) OpenSCAD library)

## Setup

### Host program

```bash
cd controller-host
make build
make run
```

Requires Go 1.21+ and macOS (uses AppleScript to control the Music app).

### Arduino

1. Open `Music_controller_arduino/Music_controller_arduino.ino` in the Arduino IDE.
2. Install the **TFT_eSPI** library and copy `Music_controller_arduino/eTFT_User_Setup.h` to the library's `User_Setup.h`.
3. Select the **Arduino Nano ESP32** board with **"Pin numbering: by GPIO (legacy)"**.
4. Use Arduino ESP32 board package **2.0.13** — newer versions are incompatible with TFT_eSPI.
5. Upload the sketch.

Plug the controller into your Mac via USB, run the host program, and it will connect automatically.

## Licence

BSD 3-Clause — see [LICENSE](LICENSE).
