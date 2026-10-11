#pragma once

// The heliostat's views (docs/DESIGN.md section 7.1): the pages TesserUI shows
// first, for the firmware and the simulator alike.

#include "tesserui/views.h"

namespace heliostat {

// The custom elements are served at /ext/<file> (heliostat/elements.h): from
// flash by WebUi::asset() on the device, from web/elements/ by the simulator.
void addViews(tesserui::Views& views);

}  // namespace heliostat
