#include "heliostat/tracker.h"

#include "tessermotion/platform/lock.h"

namespace heliostat {

using tessermotion::AxisState;
using tessermotion::Guard;

void Tracker::setAim(const Aim& aim) {
    Guard g(lock());
    aim_ = aim;
}

State Tracker::state() {
    Guard g(lock());
    return state_;
}

Direction Tracker::normal() {
    Guard g(lock());
    return normal_;
}

// Every mode but manual owns the axes: the tree refuses to move them
// (TesserMotion's claim).
void Tracker::own(Mode mode) {
    bool own = mode != Mode::Manual;
    if (own == owned_) return;
    owned_ = own;
    if (own) {
        azimuth_.claim("tracking");
        elevation_.claim("tracking");
    } else {
        azimuth_.release();
        elevation_.release();
    }
}

void Tracker::update(uint64_t nowUs) {
    (void)nowUs;
    own(aim_.mode);

    AxisState az = azimuth_.state(), el = elevation_.state();
    State st;
    if (az == AxisState::Disabled || el == AxisState::Disabled) {
        st = State::Disabled;
    } else if (az == AxisState::SensorFault || el == AxisState::SensorFault) {
        st = State::AxisFault;
    } else {
        st = State::Holding;
    }

    if (aim_.mode == Mode::Track && st != State::Disabled) {
        Direction goal;
        State tracked = State::Tracking;
        switch (aim_.status) {
            case AimStatus::WaitingTime: tracked = State::WaitingTime; break;
            case AimStatus::NoLocation: tracked = State::NoLocation; break;
            case AimStatus::NoDirection: tracked = State::NoDirection; break;
            case AimStatus::Ok:
                if (aim_.sourceIsSun && aim_.source.elevation < aim_.minSunElevation) {
                    goal = aim_.park;
                    tracked = State::Parked;
                } else if (!reflect(aim_.source, aim_.target, goal)) {
                    tracked = State::Degenerate;
                }
                break;
        }
        // Otherwise the axes hold their last targets.
        if (tracked == State::Tracking || tracked == State::Parked) {
            normal_ = goal;
            azimuth_.setTarget(goal.azimuth);
            elevation_.setTarget(goal.elevation);
        }
        // A fault says more than what the mode would do.
        if (st != State::AxisFault) st = tracked;
    }
    state_ = st;
}

const char* toString(State s) {
    switch (s) {
        case State::Holding: return "holding";
        case State::Tracking: return "tracking";
        case State::Parked: return "parked";
        case State::WaitingTime: return "waitingTime";
        case State::NoLocation: return "noLocation";
        case State::NoDirection: return "noDirection";
        case State::Degenerate: return "degenerate";
        case State::AxisFault: return "axisFault";
        case State::Disabled: return "disabled";
    }
    return "?";
}

}  // namespace heliostat
