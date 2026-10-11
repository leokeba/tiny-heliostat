# tiny-heliostat

Firmware for a small two-axis heliostat: an ESP32 that turns a mirror to reflect the sun (or any named direction) onto a target, with a web interface served by the device itself.

It is [open-heliostat](https://github.com/open-heliostat/open-heliostat/tree/helionext)'s `helionext` firmware rebuilt as an application of three libraries instead of an ESP32-sveltekit fork:

- [TesserKIT](https://github.com/leokeba/TesserKIT): Wi-Fi with a captive portal, time, OTA updates, backups, timers, users and status, under `/sys`.
- [TesserMotion](https://github.com/leokeba/TesserMotion): the TB6612 motor driver, MT6701 encoders, closed-loop axes, the keyframe sequencer and GPS.
- [TesserAPI](https://github.com/leokeba/TesserAPI) and [TesserUI](https://github.com/leokeba/TesserUI): the device's tree, reachable over HTTP and WebSocket, drawn in the browser without UI code.

This repository holds only what is specific to a heliostat: the sun's position, the mirror's normal, named directions, the modes (manual, track, sequence, Art-Net) and night parking.

> **Status:** specification written, code starting. The specification is [docs/DESIGN.md](docs/DESIGN.md).

## Hardware

A classic ESP32 DevKitC (4 MB flash), a TB6612FNG driving two geared DC motors (azimuth over elevation), an MT6701 magnetic encoder per axis on its own I2C bus, and an NMEA GPS. Pins in [DESIGN §3](docs/DESIGN.md#3-hardware).

## License

MIT
