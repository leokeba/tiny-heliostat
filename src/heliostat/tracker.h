#pragma once

// The heliostat's part of the control loop (docs/DESIGN.md sections 4.2 and
// 5.2–5.3): each Motion tick, it sets both axes' targets for the current
// mode. It knows no names and no tree: Heliostat::sync() hands it vectors
// through setAim(), from TesserKIT's task.
//
// Add it to the Motion after any sequencer and before the axes, so the axes
// act on its targets in the same tick.

#include <stdint.h>

#include "heliostat/geometry.h"
#include "tessermotion/axis.h"
#include "tessermotion/motion.h"

namespace heliostat {

enum class Mode : uint8_t { Manual, Track };

// What the selected mode is doing (DESIGN section 5.2).
enum class State : uint8_t {
    Holding,      // manual: the axes go where they are told
    Tracking,     // reflecting the source onto the target
    Parked,       // the sun is below the threshold
    WaitingTime,  // the sun's source needs a clock
    NoLocation,   // the sun's source needs latitude and longitude
    NoDirection,  // the source or the target isn't a known direction
    Degenerate,   // the source is opposite the target
    AxisFault,    // an axis has no reading
    Disabled,     // an axis is disabled
};

// Why Heliostat::sync() couldn't resolve an aim, or Ok.
enum class AimStatus : uint8_t { Ok, WaitingTime, NoLocation, NoDirection };

struct Aim {
    Mode mode = Mode::Manual;
    AimStatus status = AimStatus::Ok;
    bool sourceIsSun = false;
    Direction source;
    Direction target;
    float minSunElevation = 0;
    Direction park{180, 45};
};

class Tracker : public tessermotion::Part {
public:
    Tracker(tessermotion::Axis& azimuth, tessermotion::Axis& elevation) : azimuth_(azimuth), elevation_(elevation) {}

    // From any task: takes Motion's lock.
    void setAim(const Aim& aim);
    State state();
    // The last direction commanded in track mode (the normal, or the park).
    Direction normal();

    // Part
    void update(uint64_t nowUs) override;

private:
    void own(Mode mode);

    tessermotion::Axis& azimuth_;
    tessermotion::Axis& elevation_;
    // Under Motion's lock.
    Aim aim_;
    bool owned_ = false;
    State state_ = State::Holding;
    Direction normal_;
};

const char* toString(State s);

}  // namespace heliostat
