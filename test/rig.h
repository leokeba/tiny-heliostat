// A heliostat on the host: two axes on TesserMotion's simulated plants, the
// tracker, the /heliostat tree, and a fake clock, ticked at 50 Hz as on the
// chip (Heliostat::sync() then Motion::step(), as TesserKIT's task and the
// Motion task would).
#pragma once

#include <math.h>

#include <functional>
#include <string>
#include <string_view>

#include <ArduinoJson.h>

#include "Heliostat.h"
#include "TesserAPI.h"
#include "TesserMotion.h"
#include "check.h"
#include "plant.h"
#include "tessermotion/angle.h"

namespace {

// Paris, 2026-06-21 12:00 UTC: the sun at azimuth 184°, elevation 64.5°.
constexpr int64_t kSummerNoonMs = 1782043200000LL;
// Paris, 2026-12-21 02:00 UTC: night.
constexpr int64_t kWinterNightMs = 1797818400000LL;

struct Rig {
    Plant azPlant;
    Plant elPlant;
    tessermotion::Axis azimuth{azPlant, azPlant};
    tessermotion::Axis elevation{elPlant, elPlant};
    heliostat::Tracker tracker{azimuth, elevation};
    heliostat::Heliostat helio{tracker, azimuth, elevation};
    tessermotion::Motion motion;
    tesser::Api api;
    uint64_t nowUs = 1000000;
    uint32_t nowMs = 1000;
    int64_t epochMs;

    explicit Rig(int64_t epoch = kSummerNoonMs, float az = 90, float el = 45)
        : azPlant(az), elPlant(el), epochMs(epoch) {
        azimuth.tuning.maxSpeed = 10;
        elevation.tuning.maxSpeed = 10;
        helio.latitude = 48.8566;
        helio.longitude = 2.3522;
        motion.add(tracker);
        motion.add(azimuth);
        motion.add(elevation);
        api.mount("heliostat", helio);
        api.mount("azimuth", azimuth);
        api.mount("elevation", elevation);
        motion.begin();
        helio.start();
    }

    void tick() {
        azPlant.advance(0.02f);
        elPlant.advance(0.02f);
        nowUs += 20000;
        nowMs += 20;
        if (epochMs) epochMs += 20;
        {
            tesser::Lock lock(api);
            helio.sync(nowMs, epochMs);
        }
        motion.step(nowUs);
    }
    void run(float seconds) {
        for (int i = static_cast<int>(seconds * 50 + 0.5f); i > 0; i--) tick();
    }

    struct Reply {
        tesser::Status status;
        std::string body;
    };
    Reply request(tesser::Op op, std::string_view path, std::string_view body = {}) {
        JsonDocument doc;
        if (!body.empty()) deserializeJson(doc, body.data(), body.size());
        tesser::Request req;
        req.op = op;
        req.path = path;
        req.body = doc.as<JsonVariantConst>();
        req.client.authenticated = true;
        tesser::StringReply reply;
        api.handle(req, reply);
        return {reply.status, reply.body};
    }
    Reply get(std::string_view path) { return request(tesser::Op::Get, path); }
    Reply set(std::string_view path, std::string_view body) { return request(tesser::Op::Set, path, body); }
    std::string state() { return get("/heliostat/state").body; }
};

float apart(float a, float b) { return fabsf(tessermotion::wrap180(a - b)); }

}  // namespace
