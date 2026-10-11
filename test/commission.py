#!/usr/bin/env python3
"""First motor tests on the heliostat (docs/DESIGN.md section 3), one axis at a
time, with someone watching the mechanics. Standard library only:

    python3 test/commission.py http://192.168.1.10 [--axis azimuth|elevation] [--speed 5]

For each axis:
  1. Readings: the encoder reads, with no faults, and the axis is disabled.
  2. Direction: a short manual pulse on the disabled axis. The raw angle must
     increase for a positive output (TesserMotion's rule); if it decreases,
     the motor's `invert` is flipped. Nothing else moves.
  3. Hold: the axis is enabled where it is; it must stay there.
  4. Move: a small step (5°) and back, at a low speed.

Every step that moves a motor asks first. Any anomaly (the position running
away, a sensor fault, an error growing) and Ctrl-C disable both axes and zero
their manual outputs. Settings are restored on exit, except a corrected motor
`invert`, which is kept.
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

AXES = ("azimuth", "elevation")


class Device:
    def __init__(self, base):
        self.api = base.rstrip("/") + "/api"

    def get(self, path):
        with urllib.request.urlopen(self.api + path, timeout=3) as r:
            return json.loads(r.read() or "null")

    def put(self, path, value):
        req = urllib.request.Request(
            self.api + path, data=json.dumps(value).encode(), method="PUT",
            headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=3) as r:
                return json.loads(r.read() or "null")
        except urllib.error.HTTPError as e:
            raise RuntimeError(f"PUT {path} {value!r}: {e.code} {e.read().decode()}") from None

    def safe(self):
        """Both axes disabled, no manual output. Tries hard: called on errors."""
        for a in AXES:
            for path, value in ((f"/{a}/enabled", False), (f"/{a}/motor/output", 0)):
                for _ in range(3):
                    try:
                        self.put(path, value)
                        break
                    except Exception:
                        time.sleep(0.2)


AUTO = False  # --yes: someone watches the mechanics and agreed to every step


def ask(question, auto=True):
    if AUTO:
        print(f"{question} {'y' if auto else 'n'} (--yes)")
        return auto
    return input(f"{question} [y/N] ").strip().lower() == "y"


def apart(a, b):
    return abs((a - b + 180) % 360 - 180)


def readings(dev, axis):
    s = dev.get(f"/{axis}?exclude=tuning")
    print(f"  {axis}: position {s['position']:.2f}°, raw {s['sensor']['raw']:.2f}°, state {s['state']}, "
          f"faults {s['sensor']['faults']}, limits {s['limits']}")
    return s


def check_readings(dev, axis):
    print(f"\n[{axis}] 1. Readings")
    s = readings(dev, axis)
    if s["enabled"]:
        raise RuntimeError(f"{axis} is enabled: disable it before commissioning")
    samples = []
    for _ in range(10):
        samples.append(dev.get(f"/{axis}/sensor/raw"))
        time.sleep(0.1)
    spread = max(apart(x, samples[0]) for x in samples)
    faults = dev.get(f"/{axis}/sensor/faults")
    print(f"  10 readings within {spread:.3f}°, faults {faults}")
    if spread > 0.5:
        raise RuntimeError(f"{axis}: the encoder reading moves by {spread:.2f}° at rest")


def check_direction(dev, axis):
    print(f"\n[{axis}] 2. Direction: a manual pulse on the disabled axis")
    config = dev.get(f"/{axis}/motor/config")
    u = min(1.0, config["minOutput"] + 0.15)
    for attempt in range(3):
        if not ask(f"  Pulse the {axis} motor at +{u:.2f} for 0.4 s?"):
            return False
        before = dev.get(f"/{axis}/sensor/raw")
        try:
            dev.put(f"/{axis}/motor/output", u)
            time.sleep(0.4)
        finally:
            dev.put(f"/{axis}/motor/output", 0)
        time.sleep(0.5)
        after = dev.get(f"/{axis}/sensor/raw")
        moved = (after - before + 180) % 360 - 180
        print(f"  raw {before:.2f}° → {after:.2f}° ({moved:+.2f}°)")
        if abs(moved) < 0.3:
            u = min(1.0, u + 0.15)
            print(f"  barely moved: retrying at +{u:.2f}")
            continue
        if moved > 0:
            print(f"  positive output increases the raw angle: motor invert {config['invert']} is right")
        else:
            config["invert"] = not config["invert"]
            dev.put(f"/{axis}/motor/config", {"invert": config["invert"]})
            print(f"  positive output decreased the raw angle: motor invert set to {config['invert']}")
        return True
    raise RuntimeError(f"{axis}: the motor didn't move the encoder (wiring, minOutput, or the bridge)")


def watch(dev, axis, seconds, goal, give):
    """Polls the axis; fails if it strays more than `give`° from the path to
    `goal` or reports a fault. Returns the last state."""
    start = time.time()
    first = dev.get(f"/{axis}/position")
    worst = 0.0
    s = None
    while time.time() - start < seconds:
        s = dev.get(f"/{axis}?keys=position,target,error,output,state")
        if s["state"] == "sensorFault":
            raise RuntimeError(f"{axis}: sensor fault")
        # Distance outside the segment between where it started and the goal.
        span = apart(first, goal)
        off = max(0.0, apart(s["position"], first) - span, apart(s["position"], goal) - span)
        worst = max(worst, off)
        if off > give:
            raise RuntimeError(f"{axis}: position {s['position']:.2f}° strays {off:.1f}° (runaway?)")
        time.sleep(0.1)
    print(f"  position {s['position']:.2f}°, target {s['target']:.2f}°, error {s['error']:+.2f}°, "
          f"output {s['output']:+.2f}, state {s['state']}, worst stray {worst:.2f}°")
    return s


def check_hold(dev, axis):
    print(f"\n[{axis}] 3. Hold: enable where it is")
    if not ask(f"  Enable {axis} (it should not move)?"):
        return False
    here = dev.get(f"/{axis}/position")
    dev.put(f"/{axis}/enabled", True)
    s = watch(dev, axis, 5, here, 2.0)
    if apart(s["position"], here) > 1.0:
        raise RuntimeError(f"{axis}: drifted {apart(s['position'], here):.2f}° while holding")
    return True


def check_move(dev, axis, step):
    print(f"\n[{axis}] 4. Move {step:+.0f}° and back")
    speed = dev.get(f"/{axis}/tuning/maxSpeed")
    tolerance = dev.get(f"/{axis}/tuning/tolerance")
    for direction in (1, -1):
        here = dev.get(f"/{axis}/position")
        goal = (here + direction * step) % 360
        if not ask(f"  Move {axis} from {here:.1f}° to {goal:.1f}° at {speed:.0f} °/s?"):
            return False
        dev.put(f"/{axis}/target", round(goal, 2))
        s = watch(dev, axis, step / max(speed, 0.1) + 3, goal, 3.0)
        if apart(s["position"], goal) > max(1.0, 3 * tolerance):
            raise RuntimeError(f"{axis}: ended {apart(s['position'], goal):.2f}° from its target")
    return True


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("url")
    p.add_argument("--axis", choices=AXES, action="append")
    p.add_argument("--speed", type=float, default=5, help="maxSpeed during the tests, °/s (default 5)")
    p.add_argument("--step", type=float, default=5, help="the move test's step, ° (default 5)")
    p.add_argument("--yes", action="store_true", help="answer yes to every motion (someone is watching), "
                   "and leave the axes disabled")
    args = p.parse_args()
    global AUTO
    AUTO = args.yes
    dev = Device(args.url)

    print(f"Firmware: {dev.get('/sys/firmware/version')}, heliostat mode {dev.get('/heliostat/mode')}, "
          f"motors {dev.get('/heliostat/enabled')}")
    if dev.get("/heliostat/mode") != "manual":
        sys.exit("Put the heliostat in manual mode first (/heliostat/mode).")

    saved = {a: dev.get(f"/{a}/tuning/maxSpeed") for a in AXES}
    try:
        for a in AXES:
            dev.put(f"/{a}/tuning/maxSpeed", args.speed)
        for axis in args.axis or AXES:
            check_readings(dev, axis)
            if not check_direction(dev, axis):
                continue
            if check_hold(dev, axis):
                check_move(dev, axis, args.step)
            keep = ask(f"\n  Leave {axis} enabled?", auto=False)
            if not keep:
                dev.put(f"/{axis}/enabled", False)
        print("\nDone.")
    except KeyboardInterrupt:
        print("\nInterrupted: disabling both axes.")
        dev.safe()
    except Exception as e:
        print(f"\nSTOP: {e}\nDisabling both axes.")
        dev.safe()
        sys.exit(1)
    finally:
        for a in AXES:
            try:
                dev.put(f"/{a}/tuning/maxSpeed", saved[a])
            except Exception:
                pass


if __name__ == "__main__":
    main()
