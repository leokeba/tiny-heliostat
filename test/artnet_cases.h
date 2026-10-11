// Art-Net mode (docs/DESIGN.md section 5.6): /heliostat/artnet's values, as
// TesserKIT's /sys/artnet mappings write them.
#pragma once

#include "rig.h"

using heliostat::Direction;
using tesser::Status;

TEST(artnet_takes_over_and_drives_the_axes) {
    Rig r;
    r.run(0.5f);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
    // A console's first values select Art-Net mode (takeover is on by default).
    CHECK(r.set("/heliostat/artnet", R"({"azimuth":120,"elevation":30})").status == Status::Ok);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"artnet\"");
    for (int i = 0; i < 12; i++) {  // a console sends frames continuously
        CHECK(r.set("/heliostat/artnet/azimuth", "120").status == Status::Ok);
        r.run(1);
    }
    CHECK_EQ(r.state(), "\"artnet\"");
    CHECK(apart(r.azimuth.position(), 120) < 0.3f);
    CHECK(apart(r.elevation.position(), 30) < 0.3f);
    // The axes are Art-Net's.
    Rig::Reply busy = r.set("/elevation/target", "40");
    CHECK(busy.status == Status::Busy);
    CHECK(busy.body.find("art-net") != std::string::npos);
}

TEST(artnet_idle_holds) {
    Rig r;
    CHECK(r.set("/heliostat/artnet", R"({"azimuth":100,"elevation":40})").status == Status::Ok);
    r.run(4);
    CHECK_EQ(r.state(), "\"artnet\"");
    float az = r.azimuth.position();
    r.run(2);  // more than 5 s since the last value
    CHECK_EQ(r.state(), "\"artnetIdle\"");
    CHECK_EQ(r.get("/heliostat/mode").body, "\"artnet\"");  // leaving is explicit
    r.run(1);
    CHECK(apart(r.azimuth.position(), az) < 1);
    CHECK(r.set("/heliostat/artnet/azimuth", "110").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"artnet\"");
}

TEST(artnet_without_takeover_waits_for_the_mode) {
    Rig r;
    CHECK(r.set("/heliostat/artnet/takeover", "false").status == Status::Ok);
    CHECK(r.set("/heliostat/artnet", R"({"azimuth":120,"elevation":30})").status == Status::Ok);
    r.run(0.5f);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
    CHECK_EQ(r.state(), "\"holding\"");
    CHECK(r.set("/heliostat/mode", "\"artnet\"").status == Status::Ok);
    r.run(0.1f);
    CHECK_EQ(r.state(), "\"artnet\"");
}

TEST(artnet_as_a_target) {
    Rig r;
    CHECK(r.set("/heliostat/directions", R"([{"name":"Target","azimuth":0,"elevation":10},
                                              {"name":"Lamp","azimuth":90,"elevation":20}])")
              .status == Status::Ok);
    CHECK(r.set("/heliostat", R"({"source":"Lamp","artnet":{"apply":"target"}})").status == Status::Ok);
    for (int i = 0; i < 15; i++) {
        CHECK(r.set("/heliostat/artnet", R"({"azimuth":200,"elevation":15})").status == Status::Ok);
        r.run(1);
    }
    CHECK_EQ(r.state(), "\"artnet\"");
    Direction n;
    CHECK(heliostat::reflect({90, 20}, {200, 15}, n));
    CHECK(apart(r.azimuth.position(), n.azimuth) < 0.3f);
    CHECK(apart(r.elevation.position(), n.elevation) < 0.3f);
    // stop leaves Art-Net mode; the next value takes over again.
    CHECK(r.set("/heliostat/stop", "null").status == Status::Ok);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
}
