#include "heliostat/views.h"

#include "heliostat/elements.h"
#include "tessermotion/trees.h"

namespace heliostat {

void addViews(tesserui::Views& views) {
    auto& page = views.page("heliostat", "Heliostat");
    page.widget("x-sky", "/heliostat").hint("script", elements::kSkyPath);
    page.row()
        .node("/heliostat/mode")
        .node("/heliostat/state")
        .node("/heliostat/stop");
    page.row().node("/heliostat/source").node("/heliostat/target");
    page.row()
        .widget("readout", "/azimuth/position", "Azimuth")
        .widget("readout", "/elevation/position", "Elevation")
        .node("/heliostat/enabled");
}

void mountTree(tesser::Api& api, Heliostat& helio, tessermotion::Axis& azimuth, tessermotion::Axis& elevation,
               tessermotion::Motion& motion) {
    api.mount("heliostat", helio).label("Heliostat").ui("icon", "sun");
    api.mount("azimuth", azimuth).label("Azimuth").ui("icon", "motor").ui("group", "Axes");
    api.mount("elevation", elevation).label("Elevation").ui("icon", "motor").ui("group", "Axes");
    api.object("sys").mount("motion", motion).label("Motion");
}

}  // namespace heliostat
