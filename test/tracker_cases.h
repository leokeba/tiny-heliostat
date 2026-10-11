// Modes and tracking on the simulated plant (docs/DESIGN.md sections 5.2–5.3).
#pragma once

#include "rig.h"

using heliostat::Direction;
using tesser::Status;

TEST(tracker_boots_manual_and_holds) {
    Rig r;
    r.run(1);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
    CHECK_EQ(r.state(), "\"holding\"");
    CHECK(apart(r.azimuth.position(), 90) < 0.3f);
    CHECK(apart(r.elevation.position(), 45) < 0.3f);
    // Manual: the axes take targets from the tree.
    CHECK(r.set("/azimuth/target", "100").status == Status::Ok);
    r.run(3);
    CHECK(apart(r.azimuth.position(), 100) < 0.3f);
}

TEST(tracker_reflects_a_named_source) {
    Rig r;
    CHECK(r.set("/heliostat/directions", R"([{"name":"Target","azimuth":0,"elevation":10},
                                              {"name":"Lamp","azimuth":90,"elevation":20}])")
              .status == Status::Ok);
    CHECK(r.set("/heliostat", R"({"source":"Lamp","target":"Target","mode":"track"})").status == Status::Ok);
    r.run(15);
    Direction n;
    CHECK(heliostat::reflect({90, 20}, {0, 10}, n));
    CHECK_EQ(r.state(), "\"tracking\"");
    CHECK(apart(r.azimuth.position(), n.azimuth) < 0.3f);
    CHECK(apart(r.elevation.position(), n.elevation) < 0.3f);
    CHECK(apart(r.tracker.normal().azimuth, n.azimuth) < 0.01f);
}

TEST(tracker_follows_the_sun) {
    Rig r;
    CHECK(r.set("/heliostat/mode", "\"track\"").status == Status::Ok);
    r.run(15);
    CHECK_EQ(r.state(), "\"tracking\"");
    heliostat::SunPosition sun = heliostat::sunPosition(static_cast<double>(r.epochMs) / 1000.0, 48.8566, 2.3522);
    Direction n;
    CHECK(heliostat::reflect({static_cast<float>(sun.azimuth), static_cast<float>(sun.elevation)}, {180, 30}, n));
    CHECK(apart(r.azimuth.position(), n.azimuth) < 0.3f);
    CHECK(apart(r.elevation.position(), n.elevation) < 0.3f);
    // An hour later the mirror has followed.
    r.epochMs += 3600 * 1000;
    r.run(10);
    sun = heliostat::sunPosition(static_cast<double>(r.epochMs) / 1000.0, 48.8566, 2.3522);
    CHECK(heliostat::reflect({static_cast<float>(sun.azimuth), static_cast<float>(sun.elevation)}, {180, 30}, n));
    CHECK(apart(r.azimuth.position(), n.azimuth) < 0.3f);
    CHECK(apart(r.elevation.position(), n.elevation) < 0.3f);
}

TEST(tracker_slews_at_max_speed) {
    Rig r(kSummerNoonMs, 0, 45);
    CHECK(r.set("/heliostat/directions/Target", R"({"azimuth":0,"elevation":45})").status == Status::Ok);
    CHECK(r.set("/heliostat", R"({"source":"Target","mode":"track"})").status == Status::Ok);
    // The normal is the target itself, 45° of elevation away: 10 °/s.
    r.run(1);
    float moved = apart(r.azimuth.position(), 0) + apart(r.elevation.position(), 45);
    CHECK(moved < 12);
    r.run(5);
    CHECK(apart(r.azimuth.position(), 0) < 0.3f);
    CHECK(apart(r.elevation.position(), 45) < 0.3f);
}

TEST(tracker_parks_at_night) {
    Rig r(kWinterNightMs);
    CHECK(r.set("/heliostat/mode", "\"track\"").status == Status::Ok);
    r.run(15);  // 90° of azimuth at 10 °/s
    CHECK_EQ(r.state(), "\"parked\"");
    CHECK(apart(r.azimuth.position(), 180) < 0.3f);
    CHECK(apart(r.elevation.position(), 45) < 0.3f);
    // A lower threshold doesn't change the night.
    CHECK(r.set("/heliostat/park/minSunElevation", "-10").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"parked\"");
}

TEST(tracker_waits_for_time_and_location) {
    Rig r(0);
    CHECK(r.set("/heliostat/mode", "\"track\"").status == Status::Ok);
    r.run(1);
    CHECK_EQ(r.state(), "\"waitingTime\"");
    CHECK(apart(r.azimuth.position(), 90) < 0.3f);  // holding
    r.epochMs = kSummerNoonMs;
    r.helio.latitude = 0;
    r.helio.longitude = 0;
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"noLocation\"");
    CHECK(r.set("/heliostat/site", R"({"latitude":48.8566,"longitude":2.3522})").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"tracking\"");
}

TEST(tracker_degenerate_and_missing_directions) {
    Rig r;
    CHECK(r.set("/heliostat/directions", R"([{"name":"Target","azimuth":180,"elevation":-10},
                                              {"name":"Lamp","azimuth":0,"elevation":10}])")
              .status == Status::Ok);
    CHECK(r.set("/heliostat", R"({"source":"Lamp","mode":"track"})").status == Status::Ok);
    r.run(0.5f);
    CHECK_EQ(r.state(), "\"degenerate\"");
    // Renaming the target leaves the stored name behind.
    CHECK(r.set("/heliostat/directions/Target/name", "\"Window\"").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"noDirection\"");
    CHECK(r.set("/heliostat/target", "\"Window\"").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"degenerate\"");
}

TEST(tracker_owns_the_axes_outside_manual) {
    Rig r;
    CHECK(r.set("/heliostat/mode", "\"track\"").status == Status::Ok);
    r.run(0.5f);
    Rig::Reply busy = r.set("/azimuth/target", "10");
    CHECK(busy.status == Status::Busy);
    CHECK(busy.body.find("tracking") != std::string::npos);
    // Stop: manual, holding where the axes are, and theirs again.
    CHECK(r.set("/heliostat/stop", "null").status == Status::Ok);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"holding\"");
    CHECK(r.set("/azimuth/target", "10").status == Status::Ok);
}

TEST(tracker_enabled_switches_both_axes) {
    Rig r;
    r.run(0.5f);
    CHECK(r.set("/heliostat/enabled", "false").status == Status::Ok);
    r.run(0.1f);
    CHECK(!r.azimuth.enabled() && !r.elevation.enabled());
    CHECK_EQ(r.state(), "\"disabled\"");
    CHECK(r.set("/heliostat/enabled", "true").status == Status::Ok);
    r.run(0.5f);
    CHECK(r.azimuth.enabled() && r.elevation.enabled());
    CHECK_EQ(r.state(), "\"holding\"");
}

TEST(tracker_reports_axis_faults) {
    Rig r;
    CHECK(r.set("/heliostat/mode", "\"track\"").status == Status::Ok);
    r.run(1);
    r.azPlant.failing = true;
    r.run(0.5f);
    CHECK_EQ(r.state(), "\"axisFault\"");
    r.azPlant.failing = false;
    r.run(0.5f);
    CHECK_EQ(r.state(), "\"tracking\"");
}
