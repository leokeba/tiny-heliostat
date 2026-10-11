#include "heliostat/views.h"

#include "heliostat/elements.h"
#include "tessermotion/trees.h"

namespace heliostat {

void addViews(tesserui::Views& views) {
    auto& page = views.page("overview", "Overview").icon("home");
    page.card("Sky").widget("x-sky", "/heliostat").hint("script", elements::kSkyPath);
    auto& control = page.card("Control");
    control.node("/heliostat/mode").node("/heliostat/state");
    control.row().node("/heliostat/source").node("/heliostat/target");
    control.row()
        .widget("readout", "/azimuth/position", "Azimuth")
        .widget("readout", "/elevation/position", "Elevation");
    // Hold to move (manual mode, axes enabled): TesserMotion's jog.
    control.row()
        .widget("hold", "/azimuth/jogMinus", "Azimuth −")
        .widget("hold", "/azimuth/jogPlus", "Azimuth +");
    control.row()
        .widget("hold", "/elevation/jogMinus", "Elevation −")
        .widget("hold", "/elevation/jogPlus", "Elevation +");
    control.row().node("/heliostat/enabled").node("/heliostat/stop");
}

void mountTree(tesser::Api& api, Heliostat& helio, tessermotion::Axis& azimuth, tessermotion::Axis& elevation,
               tessermotion::Motion& motion) {
    api.mount("heliostat", helio).label("Heliostat").ui("icon", "sun");
    api.mount("azimuth", azimuth).label("Azimuth").ui("icon", "motor").ui("group", "Axes");
    api.mount("elevation", elevation).label("Elevation").ui("icon", "motor").ui("group", "Axes");
    api.mount("motion", motion).label("Control loop").ui("icon", "chart").ui("group", "Axes");
}

}  // namespace heliostat
