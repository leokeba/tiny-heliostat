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
    // `standby` may be null. When given, it is on while either axis is
    // enabled, and off otherwise.
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
    bool followGps = false;  // the site follows the GPS's fix
    bool gpsClock = true;    // the GPS sets the clock (the firmware, DESIGN section 5.7)
    float minSunElevation = 0;
    float parkAzimuth = 180;
    float parkElevation = 45;
    Apply artnetApply = Apply::Axes;
    bool artnetTakeover = true;  // a value while in another mode selects Art-Net

    // Art-Net mode's input, /heliostat/artnet/azimuth and elevation, written by
    // TesserKIT's /sys/artnet mappings. Under the API lock.
    void artnet(int coordinate, float degrees);
    float artnetValue(int coordinate) const { return coordinate ? artnetElevation_ : artnetAzimuth_; }
    // No Art-Net value for this long: hold.
    static constexpr uint32_t kArtnetIdleMs = 5000;

    // The tree's mode node, so mode changes made by code are announced.
    void modeNode(tesser::Node* n) { modeNode_ = n; }
    // The site's latitude and longitude, announced when the GPS sets them.
    void siteNodes(tesser::Node* lat, tesser::Node* lon) {
        latitudeNode_ = lat;
        longitudeNode_ = lon;
    }

    // A GPS position, as the firmware reads it from TesserMotion's NmeaGps.
    struct GpsFix {
        bool valid = false;  // a position, still current
        int satellites = 0;
        double latitude = 0;
        double longitude = 0;
    };
    // Under the API lock, as often as the GPS has news: its latest fix. With
    // followGps, a good one (kMinSatellites) further than kFollowDegrees from
    // the site in either coordinate becomes the site, so the stored settings
    // aren't rewritten at every fix.
    void gps(const GpsFix& fix);
    // The fix into the site (/heliostat/site/fromGps): false without a good one.
    bool siteFromGps();
    static constexpr int kMinSatellites = 4;
    static constexpr double kFollowDegrees = 0.001;  // about 100 m

    // After Motion::start(): applies `enabled` to the axes and the bridge.
    void start();
    bool enabled() const { return enabled_; }
    void enable(bool on);

    // Back to manual mode, holding where the axes are, with no jog and no
    // motor driven by hand: everything stops.
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
    void standby();

    Tracker& tracker_;
    tessermotion::Axis& azimuth_;
    tessermotion::Axis& elevation_;
    tessermotion::Tb6612Standby* standby_;
    bool enabled_ = false;  // a fresh heliostat's motors stay off until enabled
    bool started_ = false;
    bool bridgeOn_ = false;
    SunPosition sun_{0, -90};
    bool sunValid_ = false;
    uint32_t sunAtMs_ = 0;
    double sunLatitude_ = 0, sunLongitude_ = 0;
    void setMode(Mode m);
    tesser::Node* modeNode_ = nullptr;
    tesser::Node* latitudeNode_ = nullptr;
    tesser::Node* longitudeNode_ = nullptr;
    GpsFix gps_;
    void setSite(double lat, double lon);
    float artnetAzimuth_ = 0, artnetElevation_ = 0;
    uint32_t artnetWrites_ = 0, artnetSeen_ = 0;
    uint32_t artnetAtMs_ = 0;
    bool artnetEver_ = false;
};

// The /heliostat tree, found by argument-dependent lookup:
// api.mount("heliostat", heliostat).
void describe(tesser::Object& o, Heliostat& h);

}  // namespace heliostat
