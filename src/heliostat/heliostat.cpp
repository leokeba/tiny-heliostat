#include "heliostat/heliostat.h"

namespace heliostat {

void Heliostat::start() {
    started_ = true;
    bridgeOn_ = true;  // Tb6612Standby::begin() left it on
    apply();
}

void Heliostat::enable(bool on) {
    enabled_ = on;
    if (started_) apply();
}

void Heliostat::apply() {
    azimuth_.enable(enabled_);
    elevation_.enable(enabled_);
    standby();
}

// The bridge is out of standby while either axis is enabled, whoever enabled
// it (/heliostat/enabled, or an axis's own `enabled` in its tree), or drives
// its motor by hand (`motor/output` on a disabled axis).
void Heliostat::standby() {
    if (!standby_ || !started_) return;
    bool on = azimuth_.enabled() || elevation_.enabled() || azimuth_.output() != 0 || elevation_.output() != 0;
    if (on == bridgeOn_) return;
    bridgeOn_ = on;
    standby_->enable(on);
}

void Heliostat::setMode(Mode m) {
    if (mode == m) return;
    mode = m;
    if (modeNode_) modeNode_->changed();
}

void Heliostat::artnet(int coordinate, float degrees) {
    (coordinate ? artnetElevation_ : artnetAzimuth_) = degrees;
    artnetWrites_++;
    if (artnetTakeover) setMode(Mode::Artnet);
}

void Heliostat::stop() {
    setMode(Mode::Manual);
    Aim aim;
    tracker_.setAim(aim);  // releases the axes on the next tick
    // hold() also ends a jog and zeroes a motor driven by hand.
    azimuth_.hold();
    elevation_.hold();
}

const NamedDirection* Heliostat::find(const std::string& name) const {
    for (const NamedDirection& d : directions)
        if (d.name == name) return &d;
    return nullptr;
}

void Heliostat::sync(uint32_t nowMs, int64_t epochMs) {
    standby();
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

    if (artnetWrites_ != artnetSeen_) {
        artnetSeen_ = artnetWrites_;
        artnetAtMs_ = nowMs;
        artnetEver_ = true;
    }

    Aim aim;
    aim.mode = mode;
    aim.minSunElevation = minSunElevation;
    aim.park = {parkAzimuth, parkElevation};
    if (source == kSun) {
        aim.sourceIsSun = true;
        if (!sunValid_) {
            aim.sourceStatus = AimStatus::WaitingTime;
        } else if (latitude == 0 && longitude == 0) {
            aim.sourceStatus = AimStatus::NoLocation;
        } else {
            aim.source = {static_cast<float>(sun_.azimuth), static_cast<float>(sun_.elevation)};
        }
    } else if (const NamedDirection* s = find(source)) {
        aim.source = {s->azimuth, s->elevation};
    } else {
        aim.sourceStatus = AimStatus::NoDirection;
    }
    const NamedDirection* t = target == kSun ? nullptr : find(target);
    if (t) {
        aim.target = {t->azimuth, t->elevation};
    } else {
        aim.targetStatus = AimStatus::NoDirection;
    }
    aim.external = {artnetAzimuth_, artnetElevation_};
    aim.apply = artnetApply;
    aim.externalIdle = !artnetEver_ || static_cast<uint32_t>(nowMs - artnetAtMs_) > kArtnetIdleMs;
    tracker_.setAim(aim);
}

}  // namespace heliostat
