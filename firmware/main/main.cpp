// tiny-heliostat (docs/DESIGN.md): TesserKIT for Wi-Fi, time, updates,
// backups, timers and accounts under /sys; TesserMotion for the two axes;
// this repository's tracker and /heliostat tree.
//
// Flash it, join the "heliostat-xxxx" network from a phone, and the
// interface opens. The heliostat boots in manual mode; the axes hold where
// they are once they have a reading.
//
// For development, a gitignored main/secrets.h may define WIFI_SSID and
// WIFI_PASSWORD, and WIFI_TX_POWER (dBm) for boards with a weak supply.
#include <sys/time.h>

#include "Heliostat.h"
#include "TesserKIT.h"
#include "TesserMotion.h"
#include "board.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

using namespace tessermotion;

namespace {

tesserkit::Device dev("Heliostat");
tesserkit::System sys(dev);
tesserkit::Network net(dev);
tesserkit::Time timekeeper(dev);
tesserkit::Firmware firmware(dev);
tesserkit::Backup backup(dev);
tesserkit::Timers timers(dev);
tesserkit::Auth auth(dev);  // open until an admin account exists

Tb6612Standby bridge(board::kStandby);
Tb6612Channel azimuthMotor(bridge, {.pwm = board::kAzimuthPwm, .in1 = board::kAzimuthIn1, .in2 = board::kAzimuthIn2});
Tb6612Channel elevationMotor(bridge,
                             {.pwm = board::kElevationPwm, .in1 = board::kElevationIn1, .in2 = board::kElevationIn2});
I2cBus azimuthBus({.sda = board::kAzimuthSda, .scl = board::kAzimuthScl});
I2cBus elevationBus({.sda = board::kElevationSda, .scl = board::kElevationScl});
Mt6701 azimuthEncoder(azimuthBus);
Mt6701 elevationEncoder(elevationBus);
Axis azimuth(azimuthEncoder, azimuthMotor);
Axis elevation(elevationEncoder, elevationMotor);
Motion motion;

heliostat::Tracker tracker(azimuth, elevation);
heliostat::Heliostat helio(tracker, azimuth, elevation, &bridge);

int64_t epochMs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

}  // namespace

extern "C" void app_main() {
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    net.defaultNetwork(WIFI_SSID, WIFI_PASSWORD);
#endif
#if defined(WIFI_TX_POWER)
    net.txPower(WIFI_TX_POWER);
#endif
    timekeeper.defaultTz("CET-1CEST,M3.5.0,M10.5.0/3");  // Paris

    // Factory defaults, until the stored settings say otherwise.
    azimuth.tuning.maxSpeed = 10;  // °/s
    elevation.tuning.maxSpeed = 10;

    // The tracker sets the axes' targets before they act on them.
    motion.add(tracker);
    motion.add(azimuth);
    motion.add(elevation);

    auto& api = dev.api();
    api.mount("heliostat", helio).label("Heliostat");
    api.mount("azimuth", azimuth).label("Azimuth");
    api.mount("elevation", elevation).label("Elevation");
    api.object("sys").mount("motion", motion).label("Motion");

    // Nothing moves while new firmware is written.
    dev.onState([](tesserkit::State s) {
        if (s == tesserkit::State::Updating) motion.stopAll();
    });
    // The tree to the tracker, in TesserKIT's task (DESIGN section 4.2).
    dev.onTick([&api](uint32_t nowMs) {
        tesser::Lock lock(api);
        helio.sync(nowMs, epochMs());
    });

    dev.begin();  // loads the stored settings
    motion.start(50);
    helio.start();  // enables the axes and the bridge, unless stored off
}
