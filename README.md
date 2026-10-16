# tiny-heliostat

Firmware for a small two-axis heliostat: an ESP32 that turns a mirror to reflect the sun (or any named direction) onto a target, with a web interface served by the device itself.

It is [open-heliostat](https://github.com/open-heliostat/open-heliostat/tree/helionext)'s `helionext` firmware rebuilt as an application of three libraries instead of an ESP32-sveltekit fork:

- [TesserKIT](https://github.com/leokeba/TesserKIT): Wi-Fi with a captive portal, time, OTA updates from this repository's releases, files and backups, timers, users, the nodes nearby over ESP-NOW and on the LAN over UDP, the log and status, under `/sys`.
- [TesserMotion](https://github.com/leokeba/TesserMotion): the TB6612 motor driver, MT6701 encoders, closed-loop axes, the keyframe sequencer and GPS.
- [TesserAPI](https://github.com/leokeba/TesserAPI) and [TesserUI](https://github.com/leokeba/TesserUI): the device's tree, reachable over HTTP and WebSocket, drawn in the browser without UI code.

This repository holds only what is specific to a heliostat: the sun's position, the mirror's normal, named directions, the modes (manual, track, sequence, Art-Net) and night parking.

> **Status:** specification written, code starting. The specification is [docs/DESIGN.md](docs/DESIGN.md).

## Hardware

A classic ESP32 DevKitC (4 MB flash), a TB6612FNG driving two geared DC motors (azimuth over elevation), an MT6701 magnetic encoder per axis on its own I2C bus, and an NMEA GPS. Pins in [DESIGN §3](docs/DESIGN.md#3-hardware).

## Updates

Each push to `main` publishes a release, built against the library commits in [`deps.lock`](deps.lock) (`scripts/pin.sh` updates it). The heliostat checks for one daily, and installs it from `/sys/firmware/github`.

The [launcher](https://leokeba.github.io/tiny-heliostat/) reaches a heliostat by its address or over USB, and installs the firmware on a new board over USB (Chrome or Edge on a computer), then sets up its Wi-Fi. On a Mac, the bridge in each [release](https://github.com/leokeba/tiny-heliostat/releases/latest) (`tiny-heliostat-bridge-macos-arm64.zip` for Apple silicon, `-x64.zip` for Intel) lists the heliostats on the network and serves the same launcher on localhost, in any browser. It isn't notarized: macOS asks once to open it (*System Settings*, *Privacy & Security*, *Open Anyway*). Without either, `tiny-heliostat-esp32-factory.bin` goes at offset 0 over USB.

## License

MIT
