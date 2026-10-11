#include "heliostat/sun.h"

#include <math.h>

namespace heliostat {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;

double rad(double deg) { return deg * kRad; }
double deg(double r) { return r / kRad; }

double wrap360(double a) {
    double r = fmod(a, 360.0);
    return r < 0 ? r + 360.0 : r;
}

// NOAA's approximation of atmospheric refraction, in degrees, for a true
// elevation `e` in degrees.
double refraction(double e) {
    if (e > 85.0) return 0;
    double arcsec;
    if (e > 5.0) {
        double t = tan(rad(e));
        arcsec = 58.1 / t - 0.07 / (t * t * t) + 0.000086 / (t * t * t * t * t);
    } else if (e > -0.575) {
        arcsec = 1735.0 + e * (-518.2 + e * (103.4 + e * (-12.79 + e * 0.711)));
    } else {
        arcsec = -20.772 / tan(rad(e));
    }
    return arcsec / 3600.0;
}

}  // namespace

SunPosition sunPosition(double epochSeconds, double latitude, double longitude) {
    // Julian centuries since J2000.0.
    double jd = epochSeconds / 86400.0 + 2440587.5;
    double t = (jd - 2451545.0) / 36525.0;

    double meanLongitude = wrap360(280.46646 + t * (36000.76983 + t * 0.0003032));
    double meanAnomaly = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    double eccentricity = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    double m = rad(meanAnomaly);
    double centre = sin(m) * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin(2 * m) * (0.019993 - 0.000101 * t) +
                    sin(3 * m) * 0.000289;
    double trueLongitude = meanLongitude + centre;
    double omega = rad(125.04 - 1934.136 * t);
    double apparentLongitude = trueLongitude - 0.00569 - 0.00478 * sin(omega);
    double meanObliquity = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    double obliquity = rad(meanObliquity + 0.00256 * cos(omega));
    double declination = asin(sin(obliquity) * sin(rad(apparentLongitude)));

    // Equation of time, in minutes.
    double y = tan(obliquity / 2) * tan(obliquity / 2);
    double l0 = rad(meanLongitude);
    double eot = 4.0 * deg(y * sin(2 * l0) - 2 * eccentricity * sin(m) + 4 * eccentricity * y * sin(m) * cos(2 * l0) -
                           0.5 * y * y * sin(4 * l0) - 1.25 * eccentricity * eccentricity * sin(2 * m));

    // True solar time at the longitude, in minutes, then the hour angle.
    double minutesUtc = fmod(epochSeconds, 86400.0) / 60.0;
    if (minutesUtc < 0) minutesUtc += 1440.0;
    double solarMinutes = fmod(minutesUtc + eot + 4.0 * longitude, 1440.0);
    if (solarMinutes < 0) solarMinutes += 1440.0;
    double hourAngle = rad(solarMinutes / 4.0 - 180.0);

    double lat = rad(latitude);
    double cosZenith = sin(lat) * sin(declination) + cos(lat) * cos(declination) * cos(hourAngle);
    if (cosZenith > 1) cosZenith = 1;
    if (cosZenith < -1) cosZenith = -1;
    double elevation = 90.0 - deg(acos(cosZenith));
    // Compass azimuth: measured from south by the hour angle's sign, then
    // turned to 0 = north.
    double azimuth = wrap360(
        deg(atan2(sin(hourAngle), cos(hourAngle) * sin(lat) - tan(declination) * cos(lat))) + 180.0);

    return {azimuth, elevation + refraction(elevation)};
}

}  // namespace heliostat
