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

TEST(site_from_gps) {
    Rig r;
    using Fix = heliostat::Heliostat::GpsFix;
    // No fix, or too few satellites: refused, the site unchanged.
    CHECK(r.set("/heliostat/site/fromGps", "null").status == Status::NotAllowed);
    r.helio.gps(Fix{true, 3, 43.62, 1.42});
    CHECK(r.set("/heliostat/site/fromGps", "null").status == Status::NotAllowed);
    CHECK(r.helio.latitude == 48.8566);
    r.helio.gps(Fix{true, 7, 43.62, 1.42});
    CHECK(r.set("/heliostat/site/fromGps", "null").status == Status::Ok);
    CHECK(r.helio.latitude == 43.62 && r.helio.longitude == 1.42);
    CHECK_EQ(r.get("/heliostat/site/latitude").body, "43.62");
    // Without followGps, a new fix leaves the site.
    r.helio.gps(Fix{true, 7, 44, 2});
    CHECK(r.helio.latitude == 43.62);
}

TEST(site_follows_gps) {
    Rig r;
    using Fix = heliostat::Heliostat::GpsFix;
    // Written through the tree, the site keeps a float's precision (ArduinoJson
    // parses 43.62 as 43.6199989): about 0.1 m.
    auto at = [&r](double lat, double lon) {
        return fabs(r.helio.latitude - lat) < 1e-5 && fabs(r.helio.longitude - lon) < 1e-5;
    };
    CHECK(r.set("/heliostat/site", R"({"followGps":true,"latitude":43.62,"longitude":1.42})").status == Status::Ok);
    // Within 0.001°: kept, so the settings aren't rewritten at every fix.
    r.helio.gps(Fix{true, 7, 43.6205, 1.4195});
    CHECK(at(43.62, 1.42));
    // Further: followed, unless the fix is poor or stale.
    r.helio.gps(Fix{true, 3, 43.63, 1.42});
    r.helio.gps(Fix{false, 9, 43.63, 1.42});
    CHECK(at(43.62, 1.42));
    r.helio.gps(Fix{true, 7, 43.6215, 1.42});
    CHECK(at(43.6215, 1.42));
    // Persisted with the site.
    std::string stored = r.api.persistedState();
    Rig r2;
    CHECK(r2.api.restore(stored) == Status::Ok);
    CHECK(r2.helio.followGps && r2.helio.gpsClock && fabs(r2.helio.latitude - 43.6215) < 1e-5);
}
