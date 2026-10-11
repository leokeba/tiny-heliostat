#pragma once

// Directions and the mirror's normal (docs/DESIGN.md section 5.3). Azimuth is a
// compass bearing (0 north, clockwise), elevation is from the horizon, both in
// degrees. Vectors are x north, y east, z up. In float, like the control code
// (the ESP32 does double in software).

namespace heliostat {

struct Direction {
    float azimuth = 0;    // 0..360
    float elevation = 0;  // -90..90
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// A unit vector.
Vec3 toVector(Direction d);
// The direction of a non-zero vector; azimuth 0..360.
Direction toDirection(const Vec3& v);

// The normal a mirror needs to reflect light coming from `source` towards
// `target`: the bisector of the two directions. False when they are opposite
// (no mirror does it).
bool reflect(Direction source, Direction target, Direction& normal);

}  // namespace heliostat
