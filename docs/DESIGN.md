# tiny-heliostat design

Status: draft, written before any code (2026-10-11). This is the specification the code will follow; when the two disagree, fix one of them. Sections marked *(later)* are committed to but not part of the first release.

tiny-heliostat is [open-heliostat](https://github.com/open-heliostat/open-heliostat/tree/helionext) (branch `helionext`, local clone `~/Dev/open-heliostat`) rebuilt on [TesserKIT](https://github.com/leokeba/TesserKIT) and [TesserMotion](https://github.com/leokeba/TesserMotion) instead of an ESP32-sveltekit fork. It is TesserKIT's phase 3 (TesserKIT DESIGN §10.1, §15) and TesserMotion's first user (TesserMotion DESIGN §14, phase 5). Public repository, MIT license.

## 1. Goals

- **Same heliostat, new foundation.** What works in helionext keeps working: two DC servo axes with absolute encoders, sun tracking, named directions as source and target, keyframe sequences, Art-Net control, GPS position and time. The calibration routines and the mount tilt are dropped (§2.3).
- **Only the heliostat lives here.** Everything another device could use belongs to a library:

  | Concern | Owner |
  |---|---|
  | Wi-Fi, AP, time, OTA, backups, timers, users, status (`/sys`) | TesserKIT |
  | Motor driver (TB6612), encoder (MT6701), closed-loop axis, keyframe sequencer, GPS, the control task | TesserMotion |
  | Tree, transports, persistence; the web renderer and the `x-keyframes` element | TesserAPI, TesserUI |
  | Sun position, reflection, named directions, source and target, modes, night parking, `x-sky` | **tiny-heliostat** |
  | Art-Net input | TesserKIT if accepted as a generic service (§5.6, §13); this repository until then |

  A gap in a library is fixed there, generically, by asking its agent (§13), not worked around here.
- **No hand-written UI except `x-sky`.** The interface is TesserUI's renderer drawing the tree. The sequence editor is TesserUI's `x-keyframes` example.
- **One owner per actuator.** In helionext the tracker, two axis sequencers, the target sequencer, Art-Net and REST writes all set axis targets, from different tasks. Here a single `mode` decides who drives the axes, and the axes refuse writes from anyone else (§5.2).
- **Host-tested.** The sun position, the reflection, the modes and the tree run on the host with sanitizers, and against TesserMotion's simulated plant for closed-loop checks (§11).

### Non-goals

- Calibration (accelerometer calibration, mount orientation resolve, encoder linearization), mount tilt, relay autotune.
- Other boards than the classic ESP32 DevKitC.
- Several heliostats (helionext's remotes and ESP-NOW console): *(later)*, as TesserKIT NowTP peers.
- MQTT *(later, TesserKIT's Mqtt bridge)*. Teleplot: replaced by TesserMotion's charts.
- Settings migration from helionext: values are entered by hand after the first flash (§8).

## 2. What carries over

### 2.1 Kept

| helionext | tiny-heliostat |
|---|---|
| `HeliostatController`: `currentSource`, `currentTarget`, `targetsMap`, `reflect()` | `/heliostat`: `source`, `target`, `directions` (keyed list), the reflection without the mount transform (§5.3) |
| `sunTracker.latitude/longitude`, `getFromGPS` | `/heliostat/site`: `latitude`, `longitude`, `fromGps`, `followGps` |
| Sun position (KenWillmott/SolarPosition, float, no refraction) | NOAA's algorithm in double, with refraction (§5.4) |
| `Servo_Driver`, `Motor_Driver`, `Encoder`, `controlTask` | TesserMotion `Axis`, `Tb6612Channel`, `Mt6701`, `Motion` (its §4, §3.2): the same servo, simplified (derivative on measurement, no output ramp), its defects fixed (its §4.6) |
| `MovementSequencer` ×2, `TargetSequencer` | one TesserMotion `Sequencer` with two tracks, applied to the axes or to the target (§5.5) |
| `ArtNetService` (UDP 6454, ArtPoll reply, coarse/fine channels, takeover) | Art-Net input mapping channels to `/heliostat/artnet` (§5.6) |
| GPS (TinyGPSPlus) setting the clock | TesserMotion `NmeaGps`, polled by `sync()` for TesserKIT's `Time.external()` (§5.7) |
| "Sync Time" button | `/sys/time/set` (TesserKIT) |
| ESP32-sveltekit Wi-Fi, AP, NTP, OTA, core dump, status, analytics | TesserKIT `Network`, `Time`, `Firmware`, `System` |
| `FT_SECURITY=0` (open device) | TesserKIT `Auth`: open until an admin account exists |

### 2.2 Changed

- **Sources and targets stay one list of named directions**, as in helionext's `targetsMap`, with the computed `Sun` as an extra source.
- **Tracking runs in the control loop.** helionext re-aimed once a second, jumping each axis's target. Here the normal is computed every control tick (TesserMotion's 50 Hz) from a sun position refreshed once a second, and goes through the axes' slew (`maxSpeed`, default **10 °/s** here, against 0 in helionext). Target sequences play smoothly rather than in 1 s steps.
- **Night.** helionext reflected the sun even below the horizon. Here, below `park.minSunElevation`, the mirror goes to a park position (§5.3).
- **Tracking on/off** was `HeliostatController.enabled`, never exposed over REST. It becomes `mode` (§5.2), persisted and schedulable with TesserKIT's Timers ("track at 08:00, park at 21:00").
- **GPS time** sets the clock only from a valid fix with a date (helionext set it on every sentence, without checking the date).

### 2.3 Dropped

| helionext | Why |
|---|---|
| `AccelCalibService`, `MountOrientationResolveService`, `AccelerometerCalibration*`, the ADXL345 | Calibration; doesn't work. The ADXL345 stays on the board, uninitialized. |
| `mountOrientation` (tilt, tilt azimuth) and its transform | Leo's decision: not now. Reflection assumes a level mount. |
| Relay autotune | Not now. TesserMotion has a corrected one; the heliostat leaves it out (§13). |
| TMC5160, FastAccelStepper, closed-loop stepper, `SourceTargetSequencer`, `EncoderService`, `LightState*`, standalone motor and servo services | Dead code. |
| `RemoteService`, `ESPNowService` | Not wired to the heliostat; *(later)*. |
| `JsonStateRouter`, save maps, the Svelte interface | TesserAPI and TesserUI. |

## 3. Hardware

One board: the deployed **classic ESP32 DevKitC, 4 MB flash, no PSRAM** (helionext's `pins.h` `DEVKITC`; its `default_envs` pointing at an S3 was a mistake). Confirmed by Leo: a **TB6612FNG** drives the two geared DC motors, and each axis has an **MT6701** on its own I2C bus.

| Signal | GPIO | Part (TesserMotion) |
|---|---|---|
| Azimuth PWM / IN1 / IN2 | 16 / 17 / 5 | `Tb6612Channel` |
| Elevation PWM / IN1 / IN2 | 22 / 21 / 19 | `Tb6612Channel` |
| TB6612 STBY | 18 | `Tb6612Standby`, switched by `/heliostat/enabled` |
| I2C bus 1 SDA / SCL | 13 / 14 | azimuth `Mt6701` (the unused ADXL345 is on this bus too, at 0x53) |
| I2C bus 2 SDA / SCL | 27 / 26 | elevation `Mt6701` |
| GPS RX / TX, 9600 baud | 25 / 33 | `NmeaGps` on UART 1 |

- **Mechanics:** azimuth over elevation (CAD in `~/Dev/heliostat-system/cad`). The axes' angles are the mirror normal's azimuth (compass: 0 = north, clockwise) and elevation (0 = horizon). Each axis's `sensor/config/offset` makes them so; its limits keep the mechanics off their stops.
- **Before the first flash,** the deployed unit's encoder offsets, inverts and limits are read from helionext (`GET /rest/heliostat/azimuth`, `/elevation`) and written down: they are calibrated values, entered again by hand (§8). helionext's limits are in its output frame (offset applied); TesserMotion takes limits in the raw sensor frame (`sensor/raw`), so each one is entered as `limit − offset` (mod 360), or read again from `sensor/raw` at the mechanical stops.
- The pins are constants in `main/board.h`.

## 4. Architecture

```
 browser (TesserUI) ── HTTP / WebSocket ──▶ tesser::Api ◀── /sys: TesserKIT services
                                              │  /heliostat (here)   /azimuth /elevation /sequence /gps /sys/motion (TesserMotion)
 TesserKIT task (20 ms) ── api.poll, services, onTick:
     Heliostat::sync()  ── under the API lock: sun position (1 Hz), resolve source and target ──▶ Tracker::setAim() (Motion's lock)
                         ── pending GPS time ──▶ Time::external()

 Motion task (50 Hz, core 1) ── sensors ─▶ Sequencer ─▶ Tracker (a Motion part, here) ─▶ Axis ×2 ─▶ TB6612
                                             onFrame ─────┘   mode decides the axes' targets
```

### 4.1 Pieces of this repository

| Piece | What | Depends on |
|---|---|---|
| `sun` | NOAA solar position (§5.4), double | nothing |
| `geometry` | direction ↔ vector, reflection (§5.3), float | nothing |
| `Tracker` | the Motion part that sets the axes' targets for the current mode (§5.2–§5.6); no TesserAPI | TesserMotion (`Axis`, the part interface) |
| `Heliostat` | the `/heliostat` tree, modes, directions, sync with the tracker, the views | TesserAPI, TesserUI `Views`, TesserKIT (`Time`), `Tracker` |
| `x-sky` | the custom element (§7.2) | TesserUI's example |
| `main.cpp` | the device: TesserKIT services, TesserMotion parts, pins, mounts, `begin()` | everything |

The first three are platform-free and host-tested; `Heliostat` is host-tested with a TesserAPI tree and TesserMotion's fakes.

### 4.2 Tasks and locks

TesserMotion's concurrency rules apply (its §3.5): the API lock, then Motion's lock, never the reverse; the Motion task never takes the API lock; tree writes record requests that the next tick applies.

| Task | Runs | Locks |
|---|---|---|
| Motion (TesserMotion's) | 50 Hz, priority max−2, core 1: encoders, sequencer, **Tracker**, axes, GPS UART | Motion's |
| tesserkit | 20 ms: `api.poll()`, TesserKIT services, then our `onTick`: `Heliostat::sync()`, Art-Net (§5.6) | API, then Motion's inside `setAim()` |
| httpd, Wi-Fi | ESP-IDF's: API requests | API, then Motion's in TesserMotion's getters and setters |

- **`Heliostat::sync()`** (in TesserKIT's `onTick`, under the API lock) is the only bridge from the tree to the tracker. Once a second it computes the sun position. Every tick it resolves `source` and `target` against `directions` into unit vectors, and hands those, the mode and the park settings to the `Tracker` (`setAim()`, a copy under Motion's lock). Names never reach the Motion task, so renaming or removing a direction needs no hook: it takes effect on the next sync.
- **The Tracker is a TesserMotion `Part`** (`begin`, `sense` outside the lock, `update` under Motion's lock, `stop`), added after the sequencer and before the axes, so `update()` runs in that order. Motion's lock isn't recursive: inside `update()` the tracker calls the axes' lock-free variants (`setTargetLocked()` and the like; names to confirm when TesserMotion commits).
- **Readouts** (`normal`, `state`) are getters over the tracker's last result under Motion's lock, with `.watch()`, sampled only while someone is subscribed.
- **GPS time:** `Time::external()` takes the API lock, so it is never called from the Motion task. `sync()` polls TesserMotion's `gps.takeTime()` and passes what it gets to `Time::external()` (§5.7).

## 5. Behaviour

### 5.1 Axes

TesserMotion's `Axis` (its §4.3): PID with integral clamp and derivative on measurement, deadband, slew, soft limits, sensor fault handling, no jump at start. Its internals are in the raw sensor frame, so changing `offset` or `invert` never moves the motor, and limits are given in the raw frame. The heliostat sets factory defaults before `Motion::begin()`: `maxSpeed` 10 °/s, and limits off until measured on the unit (§3). Elevation targets are 0–90°.

- **Enabling:** TesserMotion axes boot disabled (`enabled` isn't persisted there), and refuse `target` writes (`not_allowed`) while disabled or starting. The heliostat's persisted `/heliostat/enabled` (default true) enables both axes and the TB6612's standby at start, and disables them when turned off. A heliostat therefore resumes its mode after a restart without a click, holding position until each axis has its first reading.

### 5.2 Modes

`/heliostat/mode`, persisted, decides who sets the axes' targets:

| Mode | Targets come from | Axes' own `target` and jog |
|---|---|---|
| `manual` (default) | the axes' own trees (`/azimuth/target`, jog) | accepted |
| `track` | the reflection of `source` onto `target` (§5.3) | refused `busy` |
| `sequence` | the sequence (§5.5) | refused `busy` |
| `artnet` | the latest Art-Net values (§5.6) | refused `busy` |

- **Ownership** is TesserMotion's: in every mode but `manual`, the tracker claims both axes, so a write to an axis's `target` or a jog through the tree is refused with `busy` and a message naming the mode ("the heliostat is tracking: switch to manual"). This generalizes TesserMotion's rule for sequences (its §5) to any owner (§13).
- **`/heliostat/stop`**: `mode = manual`, and each axis's `stop` (the setpoint becomes the position).
- **`/heliostat/state`** (read-only, watched) says what the mode is doing: `holding`, `tracking`, `parked` (sun below the threshold), `waitingTime` (clock not set), `noLocation`, `noDirection` (source or target not found), `degenerate` (source opposite the target), `sequence`, `artnet`, `artnetIdle` (no Art-Net value for 5 s: holding the last), `axisFault` (an axis in `sensorFault`), `disabled` (`enabled` off).

### 5.3 Tracking

Each Motion tick in `track` mode:

1. Source vector `s`: the sun (when `source` is `Sun`) or the named direction. Target vector `t`: the named direction (`Sun` isn't a valid target).
2. Direction to unit vector, azimuth a compass bearing: `x = cos el · cos az` (north), `y = cos el · sin az` (east), `z = sin el` (up).
3. Normal `n = s + t`. If `|n| < 1e-6` (source opposite target) the state is `degenerate` and the axes hold.
4. `az = atan2(y, x)` wrapped into [0, 360), `el = asin(z / |n|)`. These are the axes' targets (`Axis::setTarget()`, through the slew).
5. **Night:** when the source is the sun and its elevation is below `park.minSunElevation` (default 0°), the targets are `park.azimuth`, `park.elevation` (default 180°, 45°) and the state is `parked`.
6. Until the clock is valid (`/sys/time/synced`, or GPS), the state is `waitingTime` and the axes hold. The state is `noLocation` while the source is the sun and latitude and longitude are both exactly 0.

helionext's `reflect()` gives the same result at zero tilt: its `test_tilt_zero_equivalence` and bisector cases are ported as host tests.

### 5.4 Sun position

- **NOAA's solar position algorithm** (Meeus-based, the one behind NOAA's solar calculator): Julian century from the UTC epoch, mean longitude and anomaly, equation of centre, apparent longitude, obliquity, declination, equation of time, hour angle, zenith and azimuth, and NOAA's refraction correction. In double. About 0.01° over 1800–2100, against helionext's float32 low-order series without refraction. An error in the sun's direction is an error of the same size in the reflected beam: 0.1° is 9 cm at 50 m.
- Input: `time()` (UTC), latitude and longitude (degrees, east positive). Output: apparent azimuth (compass) and elevation.
- Computed once a second in `sync()`, outside the Motion task (doubles are emulated on the ESP32); the sun moves under 0.005° in between.

### 5.5 Sequence

One TesserMotion `Sequencer(2, {"azimuth", "elevation"})` mounted at `/sequence`, with its `x-keyframes` tree (TesserMotion §9.2), replaces helionext's two axis sequencers and its target sequencer. It isn't bound to the axes: the tracker receives its values through `onFrame`, and `/heliostat/sequence` decides what they mean:

- `apply` `axes`: the tracks are axis angles (helionext's `MovementSequencer`). The tracker sets them with `setTargetNow()`.
- `apply` `target`: the tracks are a target direction (helionext's `TargetSequencer`), and the mirror reflects the current source onto it, at the control rate. The named target in `directions` isn't modified.
- An empty track (NaN) keeps that coordinate where it is.
- **Starting** the sequence (`/sequence/start`, or a timer) switches the mode to `sequence`; leaving `sequence` mode stops it. This needs start and stop callbacks on `Sequencer` (§13).
- Azimuth frames aren't unwrapped by default (350° → 10° goes the long way, as in helionext); TesserMotion's `wrap(0, 360)` can make it the short way, as a setting *(to decide when the editor is used)*.

### 5.6 Art-Net

The heliostat's side is an input object, `/heliostat/artnet`, with writable `azimuth` and `elevation` (degrees, not persisted). Whoever receives Art-Net writes these values; the heliostat only decides what they do:

- In `artnet` mode the tracker uses them: as axis angles (`apply` `axes`, helionext's behaviour) or as a target direction to reflect onto (`apply` `target`).
- `takeover` (default true): a write while not in `artnet` mode switches to `artnet`. Leaving is explicit (choosing another mode), so a console that stops sending doesn't hand control back by surprise.
- No write for 5 s: state `artnetIdle`, the axes hold.

**The receiver** (UDP 6454, ArtPoll reply, ArtDmx on one universe, 8- or 16-bit channels scaled to a range, a gate channel with a threshold) is not heliostat-specific. It is proposed to TesserKIT as a generic service, `/sys/artnet`, mapping channels to any tree paths as Timers maps times to paths (§13). TesserKIT has accepted it in principle (its DESIGN §5.9, *(proposed)*). Until it exists, `artnet.cpp` here implements the same receiver, behaving as that spec says (writes through the API, only on change; 16-bit scaling; the gate's threshold) so it can be swapped out without changing `/heliostat`. Its mapping is fixed: to `/heliostat/artnet` and helionext's defaults (universe 0, azimuth channels 1/2 over 0–360°, elevation 3/4 over 0–90°), drained non-blocking in `onTick`.

### 5.7 GPS and time

- TesserMotion's `NmeaGps` on UART 1 (its §6.2), mounted at `/gps`.
- **Clock:** `sync()` polls `gps.takeTime()` and calls `timekeeper.external()` with it at most once a minute (TesserKIT ranks an external source above SNTP and the browser). `/heliostat/site/gpsClock` *(p)*, default true, turns it off.
- **Site:** `/heliostat/site/fromGps` copies the fix (with ≥ 4 satellites) into `latitude` and `longitude`. With `followGps`, it is done automatically whenever the fix moves more than 0.001° (about 100 m), so NVS isn't rewritten every second.

## 6. The tree

Persisted nodes are marked *(p)*. Access: reads are public; operating nodes (mode, stop, source, target, directions, sequence, axis targets) are `User`; settings (axis tuning and limits, park, site, Art-Net) are `Admin`. TesserKIT's Auth leaves everyone Admin until an admin account exists.

### 6.1 `/heliostat`

```
/heliostat              label "Heliostat", ui.widget "x-sky" (§7.2)
  mode *(p)             enum manual, track, sequence, artnet (default manual), ui.widget "segmented"
  state                 enum (§5.2), read-only, watched
  enabled *(p)          boolean (default true): the TB6612's standby pin, label "Motors"
  stop                  action, ui.style danger, ui.icon stop
  source *(p)           string ≤ 24 (default "Sun"): "Sun" or a direction's name; validated
  target *(p)           string ≤ 24 (default "Target"): a direction's name; validated
  directions *(p)       keyed list (key "name"), ≤ 16: {name ≤ 24, azimuth 0–360 °, elevation −90–90 °}
                        "Sun" is reserved. Default: [{"Target", 180, 30}]
  sun/                  azimuth, elevation (°), read-only, watched
  normal/               azimuth, elevation (°): what the tracker commands, read-only, watched
  site/ *(p)            ui.collapsed, Admin
    latitude, longitude   −90–90, −180–180 °, step 0.0001
    followGps, gpsClock   booleans (false, true)
    fromGps               action
  park/ *(p)            ui.collapsed, Admin: minSunElevation −10–30 ° (default 0), azimuth, elevation
  sequence/ *(p)        apply: enum axes, target (default axes)
  artnet/               azimuth, elevation (writable, °, watched); apply *(p)* enum axes, target; takeover *(p)* boolean
```

- `source` and `target` are checked against `directions` (and `Sun` for the source) by a validator. A direction removed later isn't refused retroactively: the state becomes `noDirection`.
- Adding, renaming and removing directions are list writes (TesserAPI §4.4). helionext's `add`, `remove`, `rename`, `set` actions go away.

### 6.2 TesserMotion and TesserKIT parts

| Path | Part | Tree |
|---|---|---|
| `/azimuth`, `/elevation` | `Axis` | TesserMotion §9.1, without `autotune/` (§13) |
| `/sequence` | `Sequencer`, tracks `azimuth`, `elevation` | TesserMotion §9.2 |
| `/gps` | `NmeaGps` | TesserMotion §9.3 |
| `/sys/motion` | `Motion` statistics, `stopAll` | TesserMotion §9.4 |
| `/sys/...` | `System`, `Network`, `Time`, `Firmware`, `Backup`, `Timers`, `Auth` | TesserKIT §5 |
| `/sys/artnet` | TesserKIT's Art-Net service, if accepted (§5.6) | — |

TesserKIT's `Network` names the device `heliostat-xxxx` (from the label "Heliostat"); the AP is in fallback mode. `Firmware` takes uploads; URL downloads (130 KB of TLS) aren't enabled. TesserKIT's `onState()` calls `motion.stopAll()` when an update starts (TesserMotion §3.2).

## 7. Interface

### 7.1 Pages

TesserUI draws the root as cards: Heliostat, Azimuth, Elevation, Sequence, GPS, and `/sys` as its own page. Views (TesserUI §8, `tesserui::Views` on `dev.api()`) put daily use first:

| View | Content |
|---|---|
| `Heliostat` (first) | `x-sky` full width; `mode`, `state`, `source`, `target`, `stop`; both axes' `position` and jog |
| `Sequence` | `x-keyframes` over `/sequence`, with `/heliostat/sequence/apply` and `loop` |

### 7.2 Custom elements

Served from flash with `dev.web().asset("/ext/<name>.js", ...)`, minified and gzipped into a header by `web/build.mjs` (one HTTP handler each, within TesserKIT's default `extraHandlers(4)`). Together under 6 KB gzipped.

- **`x-keyframes`**: TesserUI's example, unchanged. TesserMotion's sequencer tree is shaped for it.
- **`x-sky`**: TesserUI's `sky.js` example draws the sun, a target and the normal, and a click writes `{targetAz, targetEl}`. The heliostat's target is a *named* direction, so it needs:
  - to read `sun/`, `normal/`, `directions`, `source` and `target` from `/heliostat`;
  - a click on a named direction selects it as the target (shift-click: as the source); a click on the sky moves the current target direction (a write to `directions/<target>/azimuth` and `elevation`);
  - to draw the source when it isn't the sun, and the park position at night.

  Proposed to TesserUI as options of its example (§13); a copy in `web/elements/` until then.

## 8. Persistence and first flash

- TesserAPI persistence in NVS (TesserKIT §6): one record per top-level node, debounced 2 s. No filesystem.
- **Partition table:** TesserKIT's `tesserkit_4mb.csv` (two 1.94 MB OTA slots, core dump, no filesystem) replaces helionext's `min_spiffs.csv`. A partition table can't change over OTA, so the first flash is over USB (bootloader too: rollback is a bootloader setting). Updates are OTA after that.
- **Settings** aren't migrated: helionext's LittleFS files go with the old table. The offsets and limits noted before flashing (§3), the site and the directions are entered again in the interface, and saved with `/sys/backup`.

## 9. Repository and build

```
tiny-heliostat/
  docs/DESIGN.md
  CMakeLists.txt            ESP-IDF project; host tests when built with plain CMake (as TesserKIT does)
  main/                     main.cpp, board.h, Kconfig, secrets.h (gitignored)
  src/heliostat/            sun, geometry, Tracker, Heliostat, artnet (until TesserKIT has it)
  web/elements/             sky.js; build.mjs → src/heliostat/elements.h
  test/                     host tests, e2e_heliostat.py
  sdkconfig.defaults, partitions → TesserKIT's
  README.md, LICENSE (MIT), AGENTS.md (gitignored)
```

- **ESP-IDF** (≥ 5.2, TesserMotion's floor for the I2C master driver). TesserAPI, TesserUI, TesserKIT and TesserMotion found as siblings in `~/Dev` (`EXTRA_COMPONENT_DIRS`), then through the component manager once they are published.
- `sdkconfig.defaults`: TesserKIT's example's (rollback, core dump to flash, WebSocket, `-Os`).
- Conventions: the Tesser libraries' (C++17, no exceptions, no RTTI, `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` on the host, `uv` for Python, `pnpm` for the element build, Wi-Fi credentials only in gitignored `secrets.h`, no `Co-Authored-By` lines).

## 10. Footprint

| Item | Estimate |
|---|---|
| TesserAPI + TesserUI + TesserKIT services | about 1.1 MB on ESP-IDF `-Os` (TesserKIT's Arduino Minimal: 1.18 MB) |
| TesserMotion (two axes, a sequencer, GPS, their trees) | to be measured (TesserMotion §10) |
| This repository's code | 20–40 KB |
| `x-sky`, `x-keyframes` | < 6 KB |

Everything should fit a 1.94 MB slot with room; measured in phase 0 and then per phase.

## 11. Testing

1. **Host tests** (CMake, ASan, UBSan):
   - sun position against NOAA calculator values (several sites, solstices, equinoxes, near the horizon: ±0.02°), and against SolarPosition for parity (±0.2° above 10°);
   - reflection: helionext's zero-tilt cases, round trips, the degenerate case;
   - modes: ownership (`busy` on axis writes outside `manual`), `stop`, takeover, idle Art-Net, night parking, `waitingTime`, `noDirection` after a rename or removal, validators, sequence start switching the mode, both `apply` settings;
   - the persisted shape: a golden JSON of `api.persistedState()`, so a renamed setting shows as a diff;
   - the Art-Net receiver while it lives here (ArtPoll, ArtDmx, wrong universe, 8- and 16-bit channels).
2. **Closed loop on the host:** the tracker and both axes against TesserMotion's simulated plant (`tessermotion_testing`), with `/sys` on TesserKIT's fake platform (`tesserkit_fake`), over a simulated day: the normal followed within tolerance, `maxSpeed` respected, parking at sunset.
3. **On the heliostat:** `test/e2e_heliostat.py http://<ip>` over HTTP and WebSocket: the tree, modes and ownership, manual moves within tolerance, a sequence, an Art-Net packet sent from the script, persistence across a restart, tracking checked against the host's own sun computation. Motors move: limits on and `maxSpeed` low, with Leo's go-ahead (TesserMotion's AGENTS.md).

## 12. Roadmap

TesserMotion's phases 2–4 (axes, sequencer, GPS) come before the heliostat's hardware phases.

| Phase | Content | Acceptance | Needs |
|---|---|---|---|
| 0 | Repository, ESP-IDF project with TesserKIT's services and an empty `/heliostat`, host CMake; USB flash of the heliostat with the new table (offsets and limits noted first) | The heliostat is provisioned from a phone and updated over OTA; footprint measured. | — |
| 1 | `sun`, `geometry`, `Tracker`, `Heliostat` with their host tests | Host tests green; the tracker follows a simulated day on the plant model. | TesserMotion phase 1 (fakes, plant) |
| 2 | The axes and the tracker on hardware: manual, track, stop, night parking; `x-sky`; the Heliostat view (jog when TesserMotion has it) | A day of sun tracking onto a named target; timers switch modes. | TesserMotion phase 2, ownership (§13) |
| 3 | Sequence with `apply`; GPS clock and site | A looping target sequence plays smoothly; the clock and site come from GPS. | TesserMotion phases 3, 4, sequencer callbacks (§13) |
| 4 | Art-Net | A lighting console drives the mirror and takes over. | TesserKIT's Art-Net service, or ours |
| 5 | README, `e2e_heliostat.py` complete, TesserKIT DESIGN §10.1 updated to what was built | | |

## 13. Requests to other layers

| Request | Layer | For | Status |
|---|---|---|---|
| Axis ownership for any owner: `claim(name)` / `release()`, tree writes to `target` and jog refused `busy` naming the owner (generalizing the sequence rule) | TesserMotion | modes (§5.2) | accepted, phase 2 |
| A way for the application to add its own part to `Motion`'s tick, between the sequencers and the axes | TesserMotion | `Tracker` (§4.2) | already designed: `Part`, `motion.add()`; lock-free axis calls inside `update()` |
| `Sequencer` start and stop callbacks | TesserMotion | sequence mode (§5.5) | accepted, with the Sequencer (phase 3) |
| Jog on `Axis` (two `hold` actions, `jogSpeed`, a 1 s deadman, limits and claims respected) | TesserMotion | manual mode, the Heliostat view | accepted, after phase 2 |
| Autotune optional: not described (and not linked) unless the application asks | TesserMotion | §2.3 | accepted: autotune is deferred, opt-in when it comes |
| `NmeaGps::onTime()` runs in the Motion task, and TesserKIT's `Time::external()` takes the API lock: document "store, don't call", or offer a polled accessor | TesserMotion, TesserKIT | §5.7 | accepted: polled `gps.takeTime(int64_t&)` with NmeaGps (phase 4) |
| The fakes and the plant model usable by applications' tests | TesserMotion | §11 | accepted: `tessermotion_testing` target, phase 1 |
| The same for TesserKIT's fake platform | TesserKIT | §11 | done: `tesserkit_fake` (host only; `fake.install(dev)` before `dev.begin()`; `test/demo_main.cpp` shows a simulator), TesserKIT 483d178 |
| A generic Art-Net input service, `/sys/artnet`: universe, mappings `{path, coarse, fine, min, max}` and a gate channel with a threshold, ArtPoll reply, status | TesserKIT | §5.6 | accepted in principle: TesserKIT DESIGN §5.9 *(proposed)*; Leo decides when |
| TesserKIT DESIGN §10.1 points to this document; its §14 still lists keyed lists, arrays, object arguments and file nodes as in progress | TesserKIT | — | done, 483d178 (the link to this repository's URL once it exists) |
| A select whose options are a keyed list's keys (`ui.options "directions"`) | TesserUI | `source`, `target` as dropdowns | at phase 2; a validated text field and `x-sky` clicks until then |
| `x-sky` options: a named target, click to select, a source other than the sun, the park position | TesserUI example | §7.2 | at phase 2 |

## 14. Open questions

None at the moment.

## 15. Decisions

- **ESP-IDF, the classic ESP32 DevKitC only, public and MIT** (Leo, 2026-10-11).
- **No calibration, no mount tilt, no autotune, no multi-heliostat, no settings migration** for now (Leo, 2026-10-11).
- **Boot mode `manual` by default, persisted** (Leo, 2026-10-11): a fresh heliostat doesn't move until told to; once set, the mode survives restarts.
- **Night park at azimuth 180°, elevation 45°, below 0° of sun elevation; time zone Paris** (`CET-1CEST,M3.5.0,M10.5.0/3`) by default (Leo, 2026-10-11).
- **`maxSpeed` 10 °/s by default** (Leo, 2026-10-11): every mode is slewed, so a mode change never swings the mirror at full speed.
- **Generic parts in the libraries:** motors, encoders, axes, the sequencer and GPS are TesserMotion's; network, time, updates, timers and users are TesserKIT's. This repository keeps what only a heliostat needs.
- **A mode with axis ownership instead of flags:** helionext's `enabled`, Art-Net's takeover and three sequencers all wrote targets; one persisted enum makes ownership explicit, shows in the UI, and can be scheduled with Timers.
- **Tracking at the control rate through the slew**, instead of 1 Hz target jumps: smoother sequences, one motion limiter for every source of targets.
- **Own sun position in double** instead of SolarPosition: more accurate (refraction), host-testable, no Arduino dependency.
- **One sequencer with an `apply` setting** instead of three: the same data and editor cover axis moves and target animations.
- **Named directions stay one list** (sources and targets together), as in helionext: a lamp or a window can be either.
