// The /heliostat tree (docs/DESIGN.md section 6.1): validation and persistence.
#pragma once

#include "rig.h"

using tesser::Status;

TEST(tree_declares_cleanly) {
    Rig r;
    CHECK_EQ(tesser::declarationErrors(), 0);
    CHECK(r.get("/heliostat").status == Status::Ok);
    CHECK_EQ(r.get("/heliostat/source").body, "\"Sun\"");
    CHECK_EQ(r.get("/heliostat/directions/Target/elevation").body, "30");
}

TEST(tree_validates_names) {
    Rig r;
    CHECK(r.set("/heliostat/source", "\"Nowhere\"").status == Status::InvalidValue);
    CHECK(r.set("/heliostat/target", "\"Sun\"").status == Status::InvalidValue);
    CHECK(r.set("/heliostat/directions", R"([{"name":"Sun"}])").status == Status::InvalidValue);
    CHECK(r.set("/heliostat/directions", R"([{"name":""}])").status == Status::InvalidValue);
    CHECK(r.set("/heliostat/directions", R"([{"name":"Target","azimuth":400}])").status == Status::InvalidValue);
    // A refused patch changes nothing.
    CHECK(r.set("/heliostat", R"({"mode":"track","source":"Nowhere"})").status == Status::InvalidValue);
    CHECK_EQ(r.get("/heliostat/mode").body, "\"manual\"");
    // A new direction can be named as the source in the same patch? No: the
    // validator sees the list as it was. Two writes.
    CHECK(r.set("/heliostat/directions", R"([{"name":"Target"},{"name":"Lamp","azimuth":90,"elevation":5}])")
              .status == Status::Ok);
    CHECK(r.set("/heliostat/source", "\"Lamp\"").status == Status::Ok);
}

TEST(tree_persists_and_restores) {
    std::string stored;
    {
        Rig r;
        CHECK(r.set("/heliostat/directions", R"([{"name":"Target","azimuth":10,"elevation":5},
                                                  {"name":"Lamp","azimuth":90,"elevation":20}])")
                  .status == Status::Ok);
        CHECK(r.set("/heliostat", R"({"source":"Lamp","target":"Target","mode":"track","enabled":false})").status ==
              Status::Ok);
        CHECK(r.set("/heliostat/site", R"({"latitude":45.5,"longitude":-1.25})").status == Status::Ok);
        CHECK(r.set("/heliostat/park", R"({"azimuth":90,"elevation":10})").status == Status::Ok);
        stored = r.api.persistedState();
    }
    Rig r;
    std::vector<std::string> skipped;
    CHECK(r.api.restore(stored, &skipped) == Status::Ok);
    CHECK(skipped.empty());
    // The source and the target load after the directions they name.
    CHECK_EQ(r.get("/heliostat/source").body, "\"Lamp\"");
    CHECK_EQ(r.get("/heliostat/mode").body, "\"track\"");
    CHECK_EQ(r.get("/heliostat/enabled").body, "false");
    CHECK_EQ(r.get("/heliostat/directions/Lamp/azimuth").body, "90");
    CHECK(r.helio.latitude == 45.5 && r.helio.longitude == -1.25);
    CHECK(r.helio.parkAzimuth == 90 && r.helio.parkElevation == 10);
    CHECK(!r.azimuth.enabled());
}
