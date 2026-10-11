// Directions and the reflection (docs/DESIGN.md section 5.3).
#pragma once

#include <math.h>

#include "check.h"
#include "heliostat/geometry.h"

using heliostat::Direction;

namespace {

float apartDeg(float a, float b) {
    float d = fmodf(fabsf(a - b), 360.0f);
    return d > 180.0f ? 360.0f - d : d;
}

bool near(Direction a, Direction b, float tolerance = 0.01f) {
    bool polar = fabsf(a.elevation) > 89.99f;  // any azimuth at the zenith
    return fabsf(a.elevation - b.elevation) < tolerance && (polar || apartDeg(a.azimuth, b.azimuth) < tolerance);
}

}  // namespace

TEST(geometry_vectors) {
    heliostat::Vec3 north = heliostat::toVector({0, 0});
    CHECK(fabsf(north.x - 1) < 1e-6f && fabsf(north.y) < 1e-6f && fabsf(north.z) < 1e-6f);
    heliostat::Vec3 east = heliostat::toVector({90, 0});
    CHECK(fabsf(east.y - 1) < 1e-6f);
    heliostat::Vec3 up = heliostat::toVector({123, 90});
    CHECK(fabsf(up.z - 1) < 1e-6f);
    // Round trips, azimuth into 0..360.
    for (float az = 0; az < 360; az += 37.5f) {
        for (float el = -80; el <= 80; el += 20) {
            CHECK(near(heliostat::toDirection(heliostat::toVector({az, el})), {az, el}));
        }
    }
    CHECK(heliostat::toDirection(heliostat::toVector({-90, 10})).azimuth > 269.9f);
}

TEST(geometry_reflection) {
    Direction n;
    // Sun due south at 60°, target on the northern horizon: the mirror faces
    // north, tilted up 60°.
    CHECK(heliostat::reflect({180, 60}, {0, 0}, n));
    CHECK(near(n, {0, 60}));
    // Opposite directions at the same elevation: the mirror faces the zenith.
    CHECK(heliostat::reflect({90, 30}, {270, 30}, n));
    CHECK(near(n, {0, 90}));
    // The same direction: the mirror faces it.
    CHECK(heliostat::reflect({200, 25}, {200, 25}, n));
    CHECK(near(n, {200, 25}));
    // Symmetric: swapping source and target gives the same normal.
    Direction m;
    CHECK(heliostat::reflect({130, 40}, {250, 10}, n));
    CHECK(heliostat::reflect({250, 10}, {130, 40}, m));
    CHECK(near(n, m));
    // The normal bisects: equal angles to the source and the target.
    heliostat::Vec3 s = heliostat::toVector({130, 40}), t = heliostat::toVector({250, 10}),
                    v = heliostat::toVector(n);
    float cs = s.x * v.x + s.y * v.y + s.z * v.z, ct = t.x * v.x + t.y * v.y + t.z * v.z;
    CHECK(fabsf(cs - ct) < 1e-5f);
}

TEST(geometry_reflection_matches_helionext) {
    // helionext's reflect() at zero tilt: half the sum of the two vectors,
    // az = atan2(y, x), el = 90° - acos(z / |n|).
    const Direction pairs[][2] = {{{184, 64}, {180, 30}}, {{95, 5}, {300, 12}}, {{250, 40}, {10, 0}}};
    for (const auto& p : pairs) {
        heliostat::Vec3 a = heliostat::toVector(p[0]), b = heliostat::toVector(p[1]);
        float x = (a.x + b.x) / 2, y = (a.y + b.y) / 2, z = (a.z + b.z) / 2;
        float len = sqrtf(x * x + y * y + z * z);
        float az = atan2f(y, x) * 180.0f / 3.14159265f;
        if (az < 0) az += 360;
        float el = 90.0f - acosf(z / len) * 180.0f / 3.14159265f;
        Direction n;
        CHECK(heliostat::reflect(p[0], p[1], n));
        CHECK(near(n, {az, el}, 0.02f));
    }
}

TEST(geometry_reflection_degenerate) {
    Direction n{1, 2};
    CHECK(!heliostat::reflect({0, 10}, {180, -10}, n));
    CHECK(n.azimuth == 1 && n.elevation == 2);  // untouched
}
