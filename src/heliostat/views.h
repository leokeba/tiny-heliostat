#pragma once

// The heliostat's views (docs/DESIGN.md section 7.1): the pages TesserUI shows
// first, for the firmware and the simulator alike.

#include "heliostat/heliostat.h"
#include "tesserui/views.h"
#include "tessermotion/axis.h"
#include "tessermotion/motion.h"
#include "tessermotion/nmea_gps.h"
#include "tessermotion/tilt.h"

namespace heliostat {

// The custom elements are served at /ext/<file> (heliostat/elements.h): from
// flash by WebUi::asset() on the device, from web/elements/ by the simulator.
void addViews(tesserui::Views& views);

// The device's sections, in menu order, with their labels, icons and menu
// groups: /heliostat, then the axes and the control loop's statistics
// (/motion) under "Axes". Before Device::begin().
void mountTree(tesser::Api& api, Heliostat& helio, tessermotion::Axis& azimuth, tessermotion::Axis& elevation,
               tessermotion::Motion& motion);

// The GPS (/gps) and the accelerometer's tilt (/tilt), under "Sensors", for
// the firmware, after mountTree().
void mountSensors(tesser::Api& api, tessermotion::NmeaGps& gps, tessermotion::Tilt& tilt);

}  // namespace heliostat
