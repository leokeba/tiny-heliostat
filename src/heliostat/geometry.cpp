#include "heliostat/geometry.h"

#include <math.h>

namespace heliostat {

namespace {

constexpr float kRad = 3.14159265358979f / 180.0f;

}  // namespace

Vec3 toVector(Direction d) {
    float az = d.azimuth * kRad;
    float el = d.elevation * kRad;
    return {cosf(el) * cosf(az), cosf(el) * sinf(az), sinf(el)};
}

Direction toDirection(const Vec3& v) {
    float horizontal = sqrtf(v.x * v.x + v.y * v.y);
    float az = atan2f(v.y, v.x) / kRad;
    if (az < 0) az += 360.0f;
    if (az >= 360.0f) az = 0;
    return {az, atan2f(v.z, horizontal) / kRad};
}

bool reflect(Direction source, Direction target, Direction& normal) {
    Vec3 s = toVector(source);
    Vec3 t = toVector(target);
    Vec3 n{s.x + t.x, s.y + t.y, s.z + t.z};
    if (n.x * n.x + n.y * n.y + n.z * n.z < 1e-10f) return false;
    normal = toDirection(n);
    return true;
}

}  // namespace heliostat
