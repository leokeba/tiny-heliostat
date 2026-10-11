#include "heliostat/heliostat.h"

namespace heliostat {

void Heliostat::start() {
    started_ = true;
    apply();
}

void Heliostat::enable(bool on) {
    enabled_ = on;
    if (started_) apply();
}

// The bridge's standby before the axes when enabling, after when disabling,
// so a motor never sees an output the bridge would hold half on.
void Heliostat::apply() {
    if (enabled_ && standby_) standby_->enable(true);
    azimuth_.enable(enabled_);
    elevation_.enable(enabled_);
    if (!enabled_ && standby_) standby_->enable(false);
}

void Heliostat::stop() {
    mode = Mode::Manual;
    Aim aim;
    tracker_.setAim(aim);  // releases the axes on the next tick
    azimuth_.hold();
    elevation_.hold();
}

const NamedDirection* Heliostat::find(const std::string& name) const {
    for (const NamedDirection& d : directions)
        if (d.name == name) return &d;
    return nullptr;
}

void Heliostat::sync(uint32_t nowMs, int64_t epochMs) {
    bool timeValid = epochMs >= kValidAfterMs;
    bool moved = latitude != sunLatitude_ || longitude != sunLongitude_;
    if (!timeValid) {
        sunValid_ = false;
    } else if (!sunValid_ || moved || static_cast<uint32_t>(nowMs - sunAtMs_) >= 1000) {
        sun_ = sunPosition(static_cast<double>(epochMs) / 1000.0, latitude, longitude);
        sunValid_ = true;
        sunAtMs_ = nowMs;
        sunLatitude_ = latitude;
        sunLongitude_ = longitude;
    }

    Aim aim;
    aim.mode = mode;
    aim.minSunElevation = minSunElevation;
    aim.park = {parkAzimuth, parkElevation};
    const NamedDirection* t = target == kSun ? nullptr : find(target);
    if (source == kSun) {
        aim.sourceIsSun = true;
        if (!sunValid_) {
            aim.status = AimStatus::WaitingTime;
        } else if (latitude == 0 && longitude == 0) {
            aim.status = AimStatus::NoLocation;
        } else {
            aim.source = {static_cast<float>(sun_.azimuth), static_cast<float>(sun_.elevation)};
        }
    } else if (const NamedDirection* s = find(source)) {
        aim.source = {s->azimuth, s->elevation};
    } else {
        aim.status = AimStatus::NoDirection;
    }
    if (!t) {
        aim.status = AimStatus::NoDirection;
    } else {
        aim.target = {t->azimuth, t->elevation};
    }
    tracker_.setAim(aim);
}

}  // namespace heliostat
