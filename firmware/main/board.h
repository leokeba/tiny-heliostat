#pragma once

// The heliostat's board: a classic ESP32 DevKitC (docs/DESIGN.md section 3).
// helionext's pins.h, DEVKITC.

namespace board {

// TB6612FNG
constexpr int kStandby = 18;
constexpr int kAzimuthPwm = 16, kAzimuthIn1 = 17, kAzimuthIn2 = 5;
constexpr int kElevationPwm = 22, kElevationIn1 = 21, kElevationIn2 = 19;

// One MT6701 per bus (they share a fixed address). The ADXL345 (0x53) is on
// the azimuth's bus.
constexpr int kAzimuthSda = 13, kAzimuthScl = 14;
constexpr int kElevationSda = 27, kElevationScl = 26;

// NMEA GPS on UART 1, 9600 baud.
constexpr int kGpsRx = 25, kGpsTx = 33;

}  // namespace board
