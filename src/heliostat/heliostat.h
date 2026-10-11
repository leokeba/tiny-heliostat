#pragma once

// The heliostat's state of record and its tree, /heliostat (docs/DESIGN.md
// sections 4.2 and 6.1): the mode, named directions, the source and the
// target, the site and the park. sync() turns them into an Aim for the
// Tracker; it runs in TesserKIT's task (dev.onTick), under the API lock.

#include <stdint.h>

#include <string>
#include <vector>

#include "heliostat/geometry.h"
#include "heliostat/sun.h"
#include "heliostat/tracker.h"
#include "tesser/node.h"
#include "tessermotion/axis.h"
#include "tessermotion/tb6612.h"

namespace heliostat {

struct NamedDirection {
    std::string name = "New direction";
    float azimuth = 180;
    float elevation = 30;
};

class Heliostat {
public:
    // `standby` may be null (the axes are then only enabled and disabled).
    Heliostat(Tracker& tracker, tessermotion::Axis& azimuth, tessermotion::Axis& elevation,
              tessermotion::Tb6612Standby* standby = nullptr)
        : tracker_(tracker), azimuth_(azimuth), elevation_(elevation), standby_(standby) {}

    // The source that is computed rather than listed.
    static constexpr const char* kSun = "Sun";

    // Settings: factory defaults are these initializers, or values set
    // before the tree loads. Bound by the tree; read under the API lock.
    Mode mode = Mode::Manual;
    std::string source = kSun;
    std::string target = "Target";
    std::vector<NamedDirection> directions{{"Target", 180, 30}};
    double latitude = 0;
    double longitude = 0;
    float minSunElevation = 0;
    float parkAzimuth = 180;
    float parkElevation = 45;

    // After Motion::start(): applies `enabled` to the axes and the bridge.
    void start();
    bool enabled() const { return enabled_; }
    void enable(bool on);

    // Back to manual, holding where the axes are.
    void stop();

    // Every TesserKIT tick, under the API lock. `epochMs` is the clock's UTC
    // time, or 0 when it isn't set; the sun is computed at most once a second.
    void sync(uint32_t nowMs, int64_t epochMs);

    SunPosition sun() const { return sun_; }
    Tracker& tracker() { return tracker_; }

    // Clocks before this are unset (the epoch after a cold boot).
    static constexpr int64_t kValidAfterMs = 1704067200000LL;  // 2024-01-01

    const NamedDirection* find(const std::string& name) const;

private:
    void apply();

    Tracker& tracker_;
    tessermotion::Axis& azimuth_;
    tessermotion::Axis& elevation_;
    tessermotion::Tb6612Standby* standby_;
    bool enabled_ = true;
    bool started_ = false;
    SunPosition sun_{0, -90};
    bool sunValid_ = false;
    uint32_t sunAtMs_ = 0;
    double sunLatitude_ = 0, sunLongitude_ = 0;
};

// The /heliostat tree, found by argument-dependent lookup:
// api.mount("heliostat", heliostat).
void describe(tesser::Object& o, Heliostat& h);

}  // namespace heliostat
