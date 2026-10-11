// The heliostat on the host (docs/DESIGN.md section 11): the firmware's tree
// on TesserKIT's fake platform, both axes on TesserMotion's simulated plants,
// the clock running from the real one. It speaks TesserUI's demo protocol on
// stdin/stdout (one "<client> <envelope>" line each way), so TesserUI's
// device.mjs serves it with the real interface:
//
//   node ~/Dev/TesserUI/web/scripts/device.mjs --exe build/heliostat_sim --elements web/elements --port 8092
//   then http://localhost:8092/
//
// Options: --at <epoch s> starts the clock there (default: now); --speed <x>
// runs the clock x times faster, to watch a day go by; --dmx plays a console
// on Art-Net universe 0 (a slow sweep, 30 frames/s), with /sys/artnet on;
// --public makes clients Public instead of Admin.
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>

#include "Heliostat.h"
#include "TesserKIT.h"
#include "TesserMotion.h"
#include "fake_platform.h"
#include "heliostat/views.h"
#include "plant.h"

namespace {

int64_t realMs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

struct Client {
    Client(tesser::Api& api, long id, bool admin)
        : prefix(std::to_string(id) + " "), line(api, [this](const char* data, size_t len) {
              out.append(data, len);
              size_t nl;
              while ((nl = out.find('\n')) != std::string::npos) {
                  fwrite(prefix.data(), 1, prefix.size(), stdout);
                  fwrite(out.data(), 1, nl + 1, stdout);
                  out.erase(0, nl + 1);
              }
              fflush(stdout);
          }) {
        line.setAuthenticated(admin);
    }
    std::string prefix;
    std::string out;
    tesser::LineTransport line;
};

}  // namespace

int main(int argc, char** argv) {
    bool admin = true;
    bool dmx = false;
    double speed = 1;
    int64_t at = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--public") == 0) admin = false;
        if (strcmp(argv[i], "--dmx") == 0) dmx = true;
        if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) speed = atof(argv[++i]);
        if (strcmp(argv[i], "--at") == 0 && i + 1 < argc) at = atoll(argv[++i]) * 1000;
    }
    const int64_t realStart = realMs();
    const int64_t epochStart = at ? at : realStart;
    auto epochNow = [&] { return epochStart + static_cast<int64_t>(static_cast<double>(realMs() - realStart) * speed); };

    fake::Platform fake;
    fake.core.boot.version = "0.1.0";
    fake.core.boot.project = "tiny-heliostat";
    fake::Air home;
    home.ssid = "Workshop";
    home.password = "workshop-pw";
    home.rssi = -52;
    fake.wifi.air.push_back(home);
    fake.clock.epoch = epochNow();

    tesser::MemoryStorage storage;
    tesserkit::Device dev("Heliostat");
    fake.install(dev);
    dev.task(false).storage(storage);
    tesserkit::System sys(dev);
    tesserkit::Network net(dev);
    tesserkit::Time timekeeper(dev);
    tesserkit::Firmware firmware(dev);
    tesserkit::Backup backup(dev);
    tesserkit::Timers timers(dev);
    tesserkit::Auth auth(dev);
    tesserkit::ArtNet artnet(dev);
    artnet.enable(dmx)
        .universe(0)
        .map({"/heliostat/artnet/azimuth", 1, 2, 0, 360})
        .map({"/heliostat/artnet/elevation", 3, 4, 0, 90});
    net.defaultNetwork("Workshop", "workshop-pw");
    timekeeper.defaultTz("CET-1CEST,M3.5.0,M10.5.0/3");

    // The heliostat, its axes on simulated plants.
    Plant azPlant(198), elPlant(40);
    tessermotion::Axis azimuth(azPlant, azPlant);
    tessermotion::Axis elevation(elPlant, elPlant);
    tessermotion::Motion motion;
    heliostat::Tracker tracker(azimuth, elevation);
    heliostat::Heliostat helio(tracker, azimuth, elevation);
    azimuth.tuning.maxSpeed = 10;
    elevation.tuning.maxSpeed = 10;
    // helionext's site and direction, with motors on: a simulator has
    // nothing to break.
    helio.latitude = 43.62;
    helio.longitude = 1.42;
    helio.directions = {{"Target", 198, 24.5f}, {"Window", 120, 10}, {"Lamp", 300, 35}};
    helio.enable(true);
    motion.add(tracker);
    motion.add(azimuth);
    motion.add(elevation);

    auto& api = dev.api();
    api.mount("heliostat", helio).label("Heliostat");
    api.mount("azimuth", azimuth).label("Azimuth");
    api.mount("elevation", elevation).label("Elevation");
    api.object("sys").mount("motion", motion).label("Motion");
    tesserui::Views views(api);
    heliostat::addViews(views);
    dev.onTick([&](uint32_t nowMs) {
        tesser::Lock lock(api);
        helio.sync(nowMs, fake.clock.epoch);
    });

    if (!dev.begin()) return 1;
    fake.clock.sync(epochNow());  // SNTP answers at once
    motion.begin();
    helio.start();

    std::map<long, std::unique_ptr<Client>> byId;
    std::string pending;
    auto dispatch = [&](const std::string& in) {
        char* rest = nullptr;
        long id = strtol(in.c_str(), &rest, 10);
        if (rest == in.c_str() || *rest != ' ') return;
        std::string body(rest + 1);
        if (body == "close") {
            byId.erase(id);
            return;
        }
        auto& c = byId[id];
        if (!c) c.reset(new Client(api, id, admin));
        body += '\n';
        c->line.feed(body.data(), body.size());
    };
    fprintf(stderr, "heliostat_sim: serving on stdin/stdout\n");

    int64_t last = realMs();
    int64_t lastFrame = 0;
    char buf[4096];
    for (;;) {
        struct pollfd pfd = {0, POLLIN, 0};
        int ready = poll(&pfd, 1, 20);
        if (ready < 0 && errno != EINTR) break;
        if (ready > 0) {
            ssize_t n = read(0, buf, sizeof(buf));
            if (n == 0) break;
            if (n < 0 && errno != EINTR && errno != EAGAIN) break;
            if (n > 0) {
                pending.append(buf, static_cast<size_t>(n));
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    dispatch(pending.substr(0, nl));
                    pending.erase(0, nl + 1);
                }
            }
        }
        int64_t now = realMs();
        float dt = static_cast<float>(now - last) / 1000.0f;
        last = now;
        azPlant.advance(dt);
        elPlant.advance(dt);
        fake.core.now = static_cast<uint64_t>(now - realStart) + 1000;
        fake.clock.epoch = epochNow();
        if (dmx && now - lastFrame >= 33) {
            // A console sweeping azimuth 150–250° and elevation 20–50° in a minute.
            lastFrame = now;
            double t = static_cast<double>(now - realStart) / 60000.0 * 2 * 3.14159265358979;
            auto channels = [](double v, double lo, double hi) {
                auto x = static_cast<uint16_t>((v - lo) / (hi - lo) * 65535.0);
                return std::string{static_cast<char>(x >> 8), static_cast<char>(x & 0xFF)};
            };
            std::string data = channels(200 + 50 * sin(t), 0, 360) + channels(35 + 15 * cos(t), 0, 90);
            std::string p("Art-Net\0\x00\x50\x00\x0e\x01\x00\x00\x00\x00\x04", 18);
            fake.udp.deliver(6454, p + data);
        }
        fake.core.runTasks();
        dev.loop();  // TesserKIT's services, then the heliostat's sync
        motion.step(static_cast<uint64_t>(now - realStart) * 1000 + 1000000);
    }
    return 0;
}
