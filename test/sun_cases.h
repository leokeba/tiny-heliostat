// The sun's position (docs/DESIGN.md section 5.4) against NREL's SPA, the
// reference algorithm (0.0003°), as pvlib computes it (spa_python, sea level,
// 12 °C). Generated with:
//   uv run --with pvlib python: pvlib.solarposition.spa_python(time, lat, lon)
#pragma once

#include <math.h>

#include "check.h"
#include "heliostat/sun.h"

namespace {

struct SunCase {
    const char* name;
    double latitude, longitude;
    long long epoch;
    double azimuth, elevation;  // apparent
};

const SunCase kSunCases[] = {
    {"Paris 2026-06-21T12:00:00Z", 48.8566, 2.3522, 1782043200LL, 184.0528, 64.5441},
    {"Paris 2026-12-21T08:30:00Z", 48.8566, 2.3522, 1797841800LL, 135.4022, 5.3946},
    {"Paris 2026-03-20T16:45:00Z", 48.8566, 2.3522, 1774025100LL, 256.0817, 11.9849},
    {"Paris 2026-09-23T05:50:00Z", 48.8566, 2.3522, 1790142600LL, 91.3692, 1.4242},
    {"Paris 2026-01-15T15:40:00Z", 48.8566, 2.3522, 1768491600LL, 230.0880, 4.8658},
    {"Quito 2026-03-20T17:00:00Z", -0.1807, -78.4678, 1774026000LL, 87.6586, 84.6853},
    {"Quito 2026-06-21T13:00:00Z", -0.1807, -78.4678, 1782046800LL, 64.1642, 23.7411},
    {"Sydney 2026-12-21T02:00:00Z", -33.8688, 151.2093, 1797818400LL, 351.2196, 79.4578},
    {"Sydney 2026-06-21T22:30:00Z", -33.8688, 151.2093, 1782081000LL, 48.0997, 14.5255},
    {"Tromso 2026-06-21T23:00:00Z", 69.6492, 18.9553, 1782082800LL, 3.1936, 3.3388},
    {"Tromso 2026-12-21T11:00:00Z", 69.6492, 18.9553, 1797850800LL, 184.0832, -3.1435},
    {"Phoenix 2030-07-04T19:00:00Z", 33.4484, -112.074, 1909422000LL, 143.7295, 77.1500},
    {"Phoenix 2000-01-01T12:00:00Z", 33.4484, -112.074, 946728000LL, 98.6592, -30.9589},
    {"Phoenix 2026-10-11T14:20:00Z", 33.4484, -112.074, 1791728400LL, 105.1076, 9.4642},
};

double angleApart(double a, double b) {
    double d = fmod(fabs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

}  // namespace

TEST(sun_matches_spa) {
    for (const SunCase& c : kSunCases) {
        heliostat::SunPosition p = heliostat::sunPosition(static_cast<double>(c.epoch), c.latitude, c.longitude);
        // Refraction models differ near and below the horizon (SPA stops
        // correcting below -0.83°, NOAA doesn't).
        double elTolerance = c.elevation > 5 ? 0.02 : 0.15;
        // Near the zenith a small angular error is a large azimuth one.
        double azTolerance = fmin(0.02 / cos(c.elevation * 3.14159265358979 / 180.0), 0.5);
        if (!CHECK(fabs(p.elevation - c.elevation) < elTolerance) ||
            !CHECK(angleApart(p.azimuth, c.azimuth) < azTolerance)) {
            printf("  %s: azimuth %.4f (SPA %.4f), elevation %.4f (SPA %.4f)\n", c.name, p.azimuth, c.azimuth,
                   p.elevation, c.elevation);
        }
    }
}

TEST(sun_azimuth_is_a_compass_bearing) {
    // Paris, morning and evening: east of south, then west of it.
    heliostat::SunPosition morning = heliostat::sunPosition(1782018000.0, 48.8566, 2.3522);  // 05:00Z
    heliostat::SunPosition evening = heliostat::sunPosition(1782068400.0, 48.8566, 2.3522);  // 19:00Z
    CHECK(morning.azimuth > 45 && morning.azimuth < 90);
    CHECK(evening.azimuth > 270 && evening.azimuth < 315);
    CHECK(morning.azimuth >= 0 && evening.azimuth < 360);
}
