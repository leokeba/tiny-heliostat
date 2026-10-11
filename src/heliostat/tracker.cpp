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
// (TesserMotion's claim), naming the mode.
void Tracker::own(Mode mode) {
    if (mode == owner_) return;
    owner_ = mode;
    if (mode == Mode::Manual) {
        azimuth_.release();
        elevation_.release();
        return;
    }
    const char* name = mode == Mode::Artnet ? "art-net" : "tracking";
    azimuth_.claim(name);
    elevation_.claim(name);
}

void Tracker::go(Direction d) {
    normal_ = d;
    azimuth_.setTarget(d.azimuth);
    elevation_.setTarget(d.elevation);
}

// The source reflected onto `target`, or the park at night. Returns the
// state; the axes hold their last targets unless it is Tracking or Parked.
State Tracker::reflectOnto(Direction target) {
    switch (aim_.sourceStatus) {
        case AimStatus::WaitingTime: return State::WaitingTime;
        case AimStatus::NoLocation: return State::NoLocation;
        case AimStatus::NoDirection: return State::NoDirection;
        case AimStatus::Ok: break;
    }
    if (aim_.sourceIsSun && aim_.source.elevation < aim_.minSunElevation) {
        go(aim_.park);
        return State::Parked;
    }
    Direction n;
    if (!reflect(aim_.source, target, n)) return State::Degenerate;
    go(n);
    return State::Tracking;
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
    if (st == State::Disabled) {
        state_ = st;
        return;
    }

    State moded = State::Holding;
    switch (aim_.mode) {
        case Mode::Manual: break;
        case Mode::Track:
            moded = aim_.targetStatus != AimStatus::Ok ? State::NoDirection : reflectOnto(aim_.target);
            break;
        case Mode::Artnet:
            if (aim_.externalIdle) {
                moded = State::ArtnetIdle;
            } else if (aim_.apply == Apply::Axes) {
                go(aim_.external);
                moded = State::Artnet;
            } else {
                moded = reflectOnto(aim_.external);
                if (moded == State::Tracking) moded = State::Artnet;
            }
            break;
    }
    // A fault says more than what the mode would do.
    state_ = st == State::AxisFault ? st : moded;
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
        case State::Artnet: return "artnet";
        case State::ArtnetIdle: return "artnetIdle";
    }
    return "?";
}

}  // namespace heliostat
