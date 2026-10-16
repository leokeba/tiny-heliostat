// tiny-heliostat (docs/DESIGN.md): TesserKIT for Wi-Fi, time, updates,
// files and backups, timers, accounts, the log and the nodes nearby and on
// the LAN under /sys;
// TesserMotion for the two axes; this repository's tracker and /heliostat
// tree.
//
// Flash it, join the "heliostat-xxxx" network from a phone, and the
// interface opens; or reach it from its launcher
// (https://leokeba.github.io/tiny-heliostat/, pages/index.html): over USB, by
// its address, or listed under Nearby by the bridge each release carries. A fresh heliostat boots in manual mode
// with its motors off: check the axes' offsets, motor directions and limits,
// then turn /heliostat/enabled on.
//
// For development, a gitignored main/secrets.h may define WIFI_SSID and
// WIFI_PASSWORD, and WIFI_TX_POWER (dBm) for boards with a weak supply.
#include <sys/time.h>

#include "Heliostat.h"
#include "TesserKIT.h"
#include "TesserMotion.h"
#include "board.h"
#include "heliostat/elements.h"
#include "heliostat/views.h"

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
tesserkit::Files files(dev);  // LittleFS: the stored backups
tesserkit::Backup backup(dev);
tesserkit::Timers timers(dev);
tesserkit::Auth auth(dev);  // open until an admin account exists
tesserkit::ArtNet artnet(dev);
tesserkit::NowTp nowtp(dev);  // the nodes nearby over ESP-NOW (after Network: Wi-Fi first)
tesserkit::UdpApi udp(dev);   // the nodes on the LAN over UDP
tesserkit::Peers peers(dev);  // how the nodes under /peers are reached: UDP, NowTP, in which order
tesserkit::Log logs(dev);     // the firmware's log, as events and a file
// The launcher's USB way: envelopes on the console's UART, one per line, with
// Admin access (physical access). Logs stay at warnings, so that few land in
// the middle of a reply (sdkconfig.defaults).
tesser::UartTransport serialApi(dev.api(), UART_NUM_0);

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
    // Updates from this repository's releases (.github/workflows/release.yml),
    // and from any URL.
    firmware.github("leokeba/tiny-heliostat", "tiny-heliostat-{target}.bin");
    // Art-Net onto the heliostat's input, helionext's channels: off until
    // turned on, so a console on the network can't take the mirror over.
    artnet.enable(false)
        .universe(0)
        .map({"/heliostat/artnet/azimuth", 1, 2, 0, 360})
        .map({"/heliostat/artnet/elevation", 3, 4, 0, 90});

    // Factory defaults, until the stored settings say otherwise.
    azimuth.tuning.maxSpeed = 10;  // °/s
    elevation.tuning.maxSpeed = 10;

    // The tracker sets the axes' targets before they act on them.
    motion.add(tracker);
    motion.add(azimuth);
    motion.add(elevation);

    auto& api = dev.api();
    heliostat::mountTree(api, helio, azimuth, elevation, motion);
    static tesserui::Views views(api);
    heliostat::addViews(views);
    namespace el = heliostat::elements;
    dev.web().asset(el::kSkyPath, el::kSky, sizeof(el::kSky), "text/javascript", true);

    // Nothing moves while new firmware is written.
    dev.onState([](tesserkit::State s) {
        if (s == tesserkit::State::Updating) motion.stopAll();
    });
    // The tree to the tracker, in TesserKIT's task (DESIGN section 4.2).
    dev.onTick([&api](uint32_t nowMs) {
        tesser::Lock lock(api);
        helio.sync(nowMs, epochMs());
    });

    dev.cors("https://leokeba.github.io");  // the launchers (its own, TesserKIT's), reaching it by its address
    dev.begin();  // loads the stored settings
    serialApi.begin();
    motion.start(50);
    helio.start();  // enables the axes and the bridge, unless stored off
}
