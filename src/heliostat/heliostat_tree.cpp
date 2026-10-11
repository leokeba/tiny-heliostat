// The /heliostat tree (docs/DESIGN.md section 6.1).
#include <string>

#include "heliostat/heliostat.h"
#include "tesser/status.h"

namespace heliostat {

namespace {

using tesser::Access;
using tesser::Check;
using tesser::Object;
using tesser::Status;

constexpr size_t kMaxName = 24;
constexpr size_t kMaxDirections = 16;

}  // namespace

void describe(Object& o, Heliostat& h) {
    o.writeAccess(Access::User);

    h.modeNode(&o.value("mode", h.mode, {"manual", "track", "artnet"})
                    .persist()
                    .doc("Who moves the mirror: the axes' own targets, the reflection of the source onto the "
                         "target, or Art-Net")
                    .ui("widget", "segmented"));
    o.value(
         "state", [&h] { return h.tracker().state(); },
         {"holding", "tracking", "parked", "waitingTime", "noLocation", "noDirection", "degenerate", "axisFault",
          "disabled", "artnet", "artnetIdle"})
        .watch();
    o.value("enabled", [&h] { return h.enabled(); }, [&h](bool on) { h.enable(on); })
        .persist()
        .label("Motors")
        .doc("Both axes and the motor driver");
    o.action("stop", [&h] { h.stop(); }).doc("Manual mode, holding where the axes are").ui("style", "danger").ui(
        "icon", "stop");

    // Before the source and the target, so a stored state loads them first
    // and their validators find the directions they name.
    o.list("directions", h.directions,
           [](Object& d, NamedDirection& n) {
               d.value("name", n.name).validate([](const std::string& v) {
                   if (v.empty() || v.size() > kMaxName) return Check::fail(Status::InvalidValue, "1 to 24 characters");
                   if (v == Heliostat::kSun) return Check::fail(Status::InvalidValue, "\"Sun\" is the computed source");
                   return Check::ok();
               });
               d.value("azimuth", n.azimuth).range(0, 360).step(0.1).unit("°");
               d.value("elevation", n.elevation).range(-90, 90).step(0.1).unit("°");
           })
        .key("name")
        .maxSize(kMaxDirections)
        .persist()
        .doc("Named directions, usable as sources and targets");
    o.value("source", h.source)
        .validate([&h](const std::string& v) {
            if (v == Heliostat::kSun || h.find(v)) return Check::ok();
            return Check::fail(Status::InvalidValue, "\"Sun\" or a direction's name");
        })
        .persist()
        .doc("Where the light comes from")
        .ui("options", "directions")
        .ui("optionsExtra", Heliostat::kSun);
    o.value("target", h.target)
        .validate([&h](const std::string& v) {
            if (v != Heliostat::kSun && h.find(v)) return Check::ok();
            return Check::fail(Status::InvalidValue, "a direction's name");
        })
        .persist()
        .doc("Where the light goes")
        .ui("options", "directions");

    auto& sun = o.object("sun").label("Sun");
    sun.value("azimuth", [&h] { return h.sun().azimuth; }).unit("°").ui("precision", 1).watch();
    sun.value("elevation", [&h] { return h.sun().elevation; }).unit("°").ui("precision", 1).watch();

    auto& normal =
        o.object("normal").label("Mirror normal").doc("What the mode commands").ui("visibleWhen", "mode!=manual");
    normal.value("azimuth", [&h] { return h.tracker().normal().azimuth; }).unit("°").ui("precision", 1).watch();
    normal.value("elevation", [&h] { return h.tracker().normal().elevation; }).unit("°").ui("precision", 1).watch();

    auto& site = o.object("site").label("Site").persist().writeAccess(Access::Admin).ui("collapsed");
    site.value("latitude", h.latitude).range(-90, 90).step(0.0001).unit("°");
    site.value("longitude", h.longitude).range(-180, 180).step(0.0001).unit("°").doc("East positive");

    auto& artnet = o.object("artnet")
                       .label("Art-Net")
                       .doc("Art-Net mode's input: map /sys/artnet's channels here")
                       .ui("collapsed");
    artnet.value("azimuth", [&h] { return h.artnetValue(0); }, [&h](float v) { h.artnet(0, v); })
        .range(0, 360)
        .unit("°")
        .watch();
    artnet.value("elevation", [&h] { return h.artnetValue(1); }, [&h](float v) { h.artnet(1, v); })
        .range(-90, 90)
        .unit("°")
        .watch();
    artnet.value("apply", h.artnetApply, {"axes", "target"})
        .persist()
        .doc("axes: the values are the axes' angles; target: a direction the source is reflected onto");
    artnet.value("takeover", h.artnetTakeover).persist().doc("A value while in another mode selects Art-Net mode");

    auto& park = o.object("park")
                     .label("Night park")
                     .persist()
                     .writeAccess(Access::Admin)
                     .doc("Where the mirror waits while the sun is below the threshold")
                     .ui("collapsed");
    park.value("minSunElevation", h.minSunElevation).range(-10, 30).step(0.5).unit("°").label("Below sun elevation");
    park.value("azimuth", h.parkAzimuth).range(0, 360).step(0.1).unit("°");
    park.value("elevation", h.parkElevation).range(-90, 90).step(0.1).unit("°");
}

}  // namespace heliostat
