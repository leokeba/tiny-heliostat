#include "heliostat/views.h"

#include "heliostat/elements.h"

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

}  // namespace heliostat
