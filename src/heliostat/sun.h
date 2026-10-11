#pragma once

// The sun's apparent position (docs/DESIGN.md section 5.4): NOAA's solar
// position algorithm (Meeus-based, the one behind NOAA's solar calculator),
// with its refraction correction. In double: about 0.01° over 1800–2100.

namespace heliostat {

struct SunPosition {
    double azimuth;    // °, compass bearing: 0 north, clockwise, 0..360
    double elevation;  // °, apparent (refraction included), from the horizon
};

// `epochSeconds` UTC; latitude and longitude in degrees, east positive.
SunPosition sunPosition(double epochSeconds, double latitude, double longitude);

}  // namespace heliostat
