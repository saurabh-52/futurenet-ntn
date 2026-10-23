# futurenet-ntn — Problem Statement and Step-by-Step Walkthrough

This document explains **what problem the module solves** and **how every part was built**, in
the order it was built, with worked numerical examples. It is written for team members who want
to understand (or present) the project, not only use it. It is extended after every phase.

* Phase 1 (Mandatory) — sections 1–12 ✅
* Phase 2 (Recommended) — sections P2.1–P2.8 ✅
* Phase 3 (Optional) — to be added

---

## 1. Problem statement

### 1.1 In one paragraph

Satellites in **Low Earth Orbit (LEO)** — a few hundred to two thousand kilometres up — circle
the Earth in about 1.5–2 hours. A ground terminal can only talk to a satellite while that
satellite is **above its horizon** (in practice, above some minimum angle). Because satellites
move fast (≈ 7.6 km/s), each one is in view for only a few minutes, the **distance** to it (and
therefore the **signal delay**) changes all the time, and the terminal must regularly **switch
to another satellite (handover)**. ns-3 has no ready-made, simple model of this. Our task
(FutureNet module **M01**) is to build a **reusable ns-3 module** that represents exactly this
behaviour at the system level, so that later projects (terrestrial + satellite handover,
routing, edge computing, federated learning) can build on it.

### 1.2 What the SRS asks for (Mandatory part)

| SRS ID | In plain words |
|---|---|
| FR-01 | Create satellites and ground terminals easily (helper). |
| FR-02 | A satellite's position and velocity at any time (circular orbit). |
| FR-03 | Decide whether a satellite is visible from a terminal (minimum elevation angle). |
| FR-04 | Give the distance (slant range) to every visible satellite. |
| FR-05 | Turn distance into delay: distance / speed of light (+ optional processing delay). |
| FR-06 | Each terminal uses one satellite, chosen by a policy (baseline: highest in the sky). |
| FR-07 | Re-check all of this every `UpdateInterval` (default 100 ms). |
| FR-08 | Announce changes through ns-3 TraceSources. |
| FR-09 | Configurable link data rate. |

Plus: tests M01-T01…T06, validation V1–V3, two examples, documentation, Git, and the common
FutureNet engineering rules (Attributes with units and ranges, no hard-coding, reproducible
output, clang-format, etc.).

### 1.3 What it deliberately is NOT

Not a 5G/NR-NTN radio model: no waveforms, no Doppler, no beams, no fading, no standards
conformance. It is a **geometry → distance → delay → association** abstraction.

---

## 2. Key ideas (glossary with numbers)

| Term | Meaning | Example (600 km orbit) |
|---|---|---|
| Orbit radius *a* | Earth radius + altitude | 6,371 + 600 = **6,971 km** |
| Orbit period *T* | time for one lap | **5,792 s ≈ 96.5 min** |
| Angular speed *ω* | how fast the phase angle grows | 0.06215 °/s |
| Phase *u* | where on its ring the satellite is | after 1,000 s: 62.15° |
| Elevation | angle of the satellite above the local horizon | 90° = straight up, 0° = horizon |
| MinElevation | lowest elevation that counts as visible | 10° (default) |
| Slant range | straight-line terminal–satellite distance | 600 km overhead, ~1,932 km at 10° |
| Propagation delay | slant range / speed of light | 600 km → **2.0014 ms** |
| Link delay | propagation delay + processing delay | 2.0014 + 1 = 3.0014 ms |
| Association | which satellite a terminal currently uses | -1 = none |
| Handover | association switches directly from one satellite to another | |

---

## 3. The big picture

```text
 time ──► every 100 ms NtnLinkManager::Update():
          ┌──────────────────────────────────────────────────────────────┐
          │ for each ground terminal:                                    │
          │   for each satellite:  position → elevation, range, delay    │
          │                        visible?  (elevation ≥ MinElevation)   │
          │   policy.Select(visible candidates) → one satellite or -1    │
          │   fire traces: VisibilityChange, SlantRangeUpdate,           │
          │                LinkDelayUpdate, AssociationChange            │
          └──────────────────────────────────────────────────────────────┘
                  │ traces                               │ traces
                  ▼                                      ▼
      NtnIpLinkController                         NtnTraceWriter
      - p2p channel Delay = link delay            - link-state CSV
      - only associated link UP                   - common event log CSV
      - default route → current satellite
                  │
                  ▼
      real UDP packets travel over the link with the current delay
```

Why this split: the **link manager** is the *mechanism*, the **policy** makes the *decision*,
and network/file output only **listen to traces**. Any piece can be replaced without touching
the others (e.g. an RL policy, or M02 using its own network instead of our IP links).

---

## 4. Step 0 — Building the environment

**Problem:** the laptop is Windows with MinGW GCC 6.3 and no CMake; ns-3.46.1 needs C++20
(GCC ≥ 11) and is developed on Linux.

**What was done and why:**

1. Chose **Docker** (Ubuntu 24.04 image) instead of a VM — same toolchain for the whole team.
2. First image build **crashed** (`rpc error ... EOF`): ns-allinone also contains huge extra
   modules (e.g. 5G `nr`), and compiling everything with 12 parallel jobs exhausted Docker's
   ~8 GB of memory. **Fix:** build only the modules we need
   (`--enable-modules=core,network,...`) with `-j 4`. Nothing in ns-3 is modified (rule QG-07).
3. Verified the real ns-3.46.1 APIs we planned to use (e.g. `GeographicPositions::EARTH_SPHERE_RADIUS
   = 6371e3`, `EventId::IsPending()`), instead of trusting memory.
4. ns-3.46.1's style checker accepts **only clang-format 20**; Ubuntu ships 18 → installed the
   official clang-format 20 wheel from PyPI and added it to the Dockerfile.

Result: `tools/docker/Dockerfile` + `setup_guide.md` reproduce this on any laptop.

---

## 5. Step 1 — Design decisions before coding

| Question | Decision | Why |
|---|---|---|
| Coordinates? | Earth-centred Cartesian, metres; spherical Earth R = 6,371 km | Simple vector maths; matches ns-3's own sphere |
| Earth rotation? | ignored | keeps orbits analytic; documented limitation |
| Orbit period? | T = 2π√(a³/μ) | physically consistent, no extra parameter (SRS §7 option a) |
| Altitude unit? | metres in the Attribute | SRS table says km but SRS method says metres; SI rule wins |
| Links? | one PointToPoint link per terminal–satellite pair | real ns-3 packets; SRS allows abstract IP links |
| Traffic destination without a gateway? | one shared "service address" on every satellite | packets go to "my current satellite" with no app changes at handover |
| Common types? | own `NtnLinkState` / `NtnMobilityState` with the same fields as the guideline contract | instructor package `futurenet-common` not supplied |

A useful check done *before* coding: computing reference numbers with Python. This found that
the prompt's "period at 600 km ≈ 5,790 s ± 1 s" is too tight — the exact formula gives
**5,792.33 s** — so the test uses that value.

---

## 6. Step 2 — Constants, common types and input checking

**Files:** `model/ntn-constants.h`, `model/ntn-common-types.h`, `model/ntn-attribute-checkers.*`

* All physical constants and SRS ranges are **named** (`EARTH_RADIUS_M`,
  `SPEED_OF_LIGHT_M_PER_S`, `MAX_ORBIT_ALTITUDE_M`, …) — no "magic numbers" in the algorithms.
* Every configurable value is an ns-3 **Attribute** with a **bounded checker**. Example:

  ```cpp
  .AddAttribute("OrbitAltitude", "Orbit altitude above the Earth surface [m] (200,000 - 2,000,000 m).",
                DoubleValue(600e3), MakeDoubleAccessor(&Set..., &Get...),
                MakeDoubleChecker<double>(200e3, 2000e3))
  ```

  `--altitudeKm=100` → the run stops with
  `Invalid value for attribute set (OrbitAltitude)` instead of silently simulating nonsense.
* ns-3 has bounded checkers for numbers and `Time`, but **not for `DataRate`**, so we wrote a
  small `MakeNtnDataRateChecker(1 Mbit/s, 10 Gbit/s)`.

---

## 7. Step 3 — Where is the satellite? (`CircularOrbitMobilityModel`)

**Idea:** put the satellite on a flat circle, tilt the circle, then turn it.

1. In its own orbit plane, after phase *u*: `(a·cos u, a·sin u, 0)`.
2. Tilt by inclination *i* (rotate about x): `(x, y·cos i, y·sin i)`.
3. Turn by RAAN Ω (rotate about z, "which way the ring faces").

**Worked example** (600 km, i = 53°, Ω = 0, start phase 0):

* ω = 2π / 5,792.33 s = 0.0010847 rad/s = 0.06215 °/s
* at t = 1,000 s: u = 62.15°
  * in-plane: (6,971,000·cos 62.15°, 6,971,000·sin 62.15°, 0) = (3,256,442, 6,163,638, 0)
  * after tilting by 53°: **(3,256,442 m, 3,709,370 m, 4,922,500 m)**
  * check: length = 6,971,000 m = *a* ✔
* at t = T/4 (u = 90°): (0, a·cos 53°, a·sin 53°) = **(0, 4,195,252.5, 5,567,288.1) m** — this
  exact value is what test M01-T01 compares against.

**Design choice:** `GetPositionAt(t)` is a **pure function of time** — no stored state — so the
same time always gives exactly the same position (makes M01-T01 and M01-T06 easy).

The velocity is the derivative (speed = a·ω = 7,561.7 m/s, always perpendicular to the position
— also checked by a test).

---

## 8. Step 4 — Where is the terminal? (`NtnGroundTerminalMobilityModel`)

A terminal is configured with `Latitude`, `Longitude`, `GroundHeight` (0–5,000 m). Conversion to
Cartesian reuses ns-3's `GeographicPositions` (spherical Earth):

`x = R·cos(lat)·cos(lon)`, `y = R·cos(lat)·sin(lon)`, `z = R·sin(lat)`

**Example:** New Delhi (28.6° N, 77.2° E) → (1,239,260 m, 5,454,624 m, 3,049,746 m).
Latitude 0 / longitude 0 → (6,371,000, 0, 0) — used in most tests.

---

## 9. Step 5 — Can the terminal see it, how far, how long? (visibility + delay)

**Files:** `model/ntn-visibility-model.*`, `model/ntn-propagation-delay-model.*`

* line of sight `d = s − g`; slant range = |d|
* "up" at the terminal = g / |g|
* sin(elevation) = (d · up) / |d|
* visible ⇔ elevation ≥ MinElevation
* delay = range / c + ProcessingDelay

**Worked example:** terminal at (0°, 0°), satellite on an equatorial 600 km orbit at phase 10°.

| Step | Value |
|---|---|
| g | (6,371,000, 0, 0) |
| s = a·(cos 10°, sin 10°, 0) | (6,865,094.8, 1,210,501.4, 0) |
| d = s − g | (494,094.8, 1,210,501.4, 0) |
| slant range = \|d\| | **1,307,456.9 m** |
| d · up | 494,094.8 (up = x axis here) |
| sin(elev) = 494,094.8 / 1,307,456.9 | 0.3779 → **elevation 22.2°** → visible (≥ 10°) |
| propagation delay = 1,307,456.9 / 299,792,458 | **4.3612 ms** |
| link delay (+1 ms) | **5.3612 ms** |

How long is a satellite visible? At 10° elevation the terminal–satellite Earth-centre angle is
λ = acos(R·cos 10° / a) − 10° = 15.84°, so a pass straight overhead lasts
2 × 15.84° / 0.06215 °/s ≈ **510 s (8.5 min)**. With 12 satellites spaced 30° apart, a new one
rises before the old one sets → continuous coverage with a handover about every 8 minutes.

---

## 10. Step 6 — Which satellite to use? (policy + link manager)

**Files:** `model/ntn-association-policy.*`, `model/ntn-link-manager.*`

The **link manager** builds, for each terminal, a list of visible candidates and asks the
policy:

```cpp
NtnAssociationState state;          // time, terminal, current satellite, candidates
state.visibleCandidates = {{5, 20.0°, 1500 km, ...}, {3, 60.0°, 700 km, ...}, {9, 45.0°, 900 km, ...}};
policy->Select(state).satelliteId   // HighestElevationPolicy → 3 (60° is highest)
```

Ties go to the lowest node ID, an empty list gives -1. The manager **rejects** any decision
that is not a visible candidate — so even a future buggy policy can never associate with an
invisible satellite (this guarantees SRS V3).

**Example timeline** (basic example: terminal at 0°/0°, satellite starting 30° before overhead,
updates every 100 ms):

| t [s] | elevation | range [km] | delay [ms] | event |
|---|---|---|---|---|
| 0 | −5.5° | 3,501 | 12.68 | not visible |
| 227.9 | 10.0° | 1,932 | 7.44 | `VisibilityChange(visible)`, `AssociationChange(-1 → 0)` |
| 482 | ~90° | 600 | 3.00 | closest point |
| 737.5 | 10.0° | 1,932 | 7.44 | `VisibilityChange(not visible)`, `AssociationChange(0 → -1)` |

The predicted rise time (30° − 15.84°) / 0.06215 °/s = 227.9 s matches exactly.

Trace sources (for any other module to subscribe to): `VisibilityChange`, `AssociationChange`,
`SlantRangeUpdate`, `LinkDelayUpdate`, `UpdateCompleted`.

---

## 11. Step 7 — Real packets and output files

### 11.1 IP links (`NtnIpLinkController`, created by `NtnHelper::InstallIpLinks`)

* One ns-3 `PointToPoint` link per (terminal, satellite) pair, /30 subnet each, data rate =
  `DataRate` Attribute. 20 terminals × 12 satellites = 240 links.
* On `LinkDelayUpdate` → channel `Delay` = computed link delay.
* On `AssociationChange` → old link interfaces DOWN, new link UP, default route moved.
* Every satellite also owns `10.255.255.1`; a terminal sending there reaches its current
  satellite.

**Example:** a 512-byte UDP packet at 100 Mbit/s: on-the-wire 512 + 8 (UDP) + 20 (IP) + 2 (PPP)
= 542 bytes → transmission time 43.36 µs. If the link delay is 3.1515 ms, the packet arrives
3.1515 ms + 0.0434 ms after it was sent. Test T6 checks this to **1 ns**.

### 11.2 CSV files (`NtnTraceWriter`, enabled by `NtnHelper::EnableTracing`)

Link-state CSV (one row per pair per sample):

```text
time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated
228.000000,1,0,1,10.010283,1930.919597,7.440854,1
```

Common event log (same first columns for every FutureNet module):

```text
time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail
227.900000,futurenet-ntn,AssociationChange,1,0,,-1,7.443125,old_satellite_id|delay_ms,acquired
```

Writing every pair every 100 ms for 20 × 12 over 20 min would be ~150 MB, so the link-state CSV
has a sample interval (examples use 1 s); events are never sampled.

### 11.3 Helper and examples

`NtnHelper` wires everything with explicit steps (`InstallSatellites`, `InstallGroundTerminal`,
`InstallIpLinks`, `EnableTracing`, `Start`). Examples:

* `ntn-example-basic` — the timeline of section 10 as a console table + CSVs.
* `ntn-example-advanced --numGround=20 --numSats=12` — 20 terminals sending 1 packet/s each,
  FlowMonitor statistics: **68 association changes, 48 handovers, mean delay 4.5–5 ms**, runs in
  ~3 s.
* `ntn-validation` — V1–V3 (section 13).
* `plot_ntn.py` — range / delay / associated satellite vs time.

---

## 12. Step 8 — Testing (11 test cases, `./test.py -s futurenet-ntn`)

| Test | What it proves | Example check |
|---|---|---|
| T1 defaults | every Attribute starts at the SRS value | MinElevation == 10 |
| T2 / M01-T02 | bad inputs rejected, edges accepted | altitude 199,999 m rejected, 200,000 m accepted |
| T3 / M01-T01 | position = hand-computed value | y at T/4 = 4,195,252.526 m ± 1 mm |
| T3 period | T(600 km) = 5,792.3 s ± 1 s | |
| T3 / M01-T03 | visibility flips at the boundary | 0.01° inside → visible, 0.01° outside → not |
| T3 / M01-T04 | delay = range / c | within 1 ns |
| T3 / M01-T05 | policy picks highest elevation | {20°, 60°, 45°} → the 60° one |
| T4 traces | every trace fires with correct arguments | range argument equals independent geometry |
| T5 / M01-T06 | same run twice → byte-identical CSV | `ReadFile(a) == ReadFile(b)` |
| T6 integration | real packets see delay + tx time | arrival time within 1 ns |
| V2/V3/AC-05 | pass shape, valid associations, ≥ 1 handover | |

**Are the tests strong enough?** A *mutation check* was done: 5 ns was secretly added to every
delay → the suite **failed** → change reverted. So the 1 ns assertions really catch errors.

Problems met while testing: ns-3.46.1's `MobilityModel` requires a `Copy()` method (added); a
test name containing "/" is rejected by ns-3 (renamed).

---

## 13. Step 9 — Validation (is the behaviour sensible?)

Run: `./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"`.
Each case compares simulation with a closed-form formula:

| Case | Setup | Simulated | Closed form | Result |
|---|---|---|---|---|
| V1 | satellite overhead, 600 km | 3.001385 ms | 600 km / c + 1 ms = 3.0013846 ms | error 0.43 ns ✔ |
| V2 | overhead, 200/600/1200/2000 km | 0.667 / 2.001 / 4.003 / 6.671 ms | h / c | increasing ✔ |
| V2 (SRS) | one pass | range 3,501 → 600.3 → 3,739 km | falls then rises | ✔ |
| V3 | MinElevation 45°, 200 km | visible 50.6 s | 2λ/ω = 50.607 s | ✔ |
| V3 | MinElevation 0°, 2,000 km | visible 1,712.5 s, max range 5,429.9 km | 1,712.46 s, 5,429.9 km | ✔ |
| V3 (SRS) | 20 × 12, 20 min | 68 association changes, 0 invalid | must be 0 | ✔ |

(The 0.43 ns in V1 is ns-3's 1 ns time resolution, not a model error.)

---

## 14. Step 10 — Quality, documentation and Git

* **Style:** `utils/check-style-clang-format.py --fix` (clang-format 20) → clean. **Warnings:** 0.
* **Docs:** README, `doc/futurenet-ntn.rst` (formulas, parameter table, traces, limitations),
  technical report, demo instructions, requirements traceability, this walkthrough, setup guide.
* **Git:** one `feature/*` branch per part (common-types, orbit-model, visibility-delay,
  association-policy, link-manager, ip-links-tracing, helper, tests, examples, docs,
  validation-results), each merged into `develop` with `--no-ff`; `develop` merged into `main`
  at the end of the phase. `main` was then **cloned into a fresh ns-3 tree and built from
  scratch**: 0 warnings, all tests pass.

Natural split for a 4-person team: (1) orbit + geometry, (2) policy + link manager,
(3) IP links + tracing + helper + examples, (4) tests + validation + docs.

---

## 15. Phase 1 result in one table

| Acceptance criterion | Result |
|---|---|
| AC-01 builds with ns-3.46.1 | ✔ 0 warnings, clean checkout |
| AC-02 all Mandatory requirements | ✔ FR-01 … FR-09 |
| AC-03 tests pass | ✔ 11 / 11 |
| AC-04 delay numerically correct | ✔ 0.43 ns error |
| AC-05 ≥ 1 association change | ✔ 48 handovers in the 20 × 12 demo |
| AC-06 configurable | ✔ Attributes + helper + command line |
| AC-07 limitations documented | ✔ |

Known limitations: spherical non-rotating Earth, circular orbits, purely geometric visibility
(no link budget), break-before-make handover (a packet in flight on the old link is lost),
decisions every `UpdateInterval`, single orbit plane and no gateway/ISL yet.

---

## Phase 2 — Recommended requirements

### P2.1 What Phase 2 asks for, in plain words

| ID | In plain words |
|---|---|
| M01-FR-10 | Not one ring of satellites but several rings (**orbital planes**) facing different directions, with a configurable shift between the rings — a "Walker" constellation like Starlink/OneWeb use. |
| M01-FR-11 | **Gateways**: ground stations connected to the terrestrial network. A satellite relays a terminal's traffic down to a gateway it can see, so the path is terminal → satellite → gateway. |
| Policy abstraction | Choosing the satellite must be swappable; add a second rule and compare how often each one hands over. |
| `MaxAssociationsPerGround` | A terminal may keep links to 1–4 satellites at the same time (default 1 = Phase 1). |
| Integration readiness | Other teams (M02 routing, M03 …) must be able to use the module only through its public functions, traces and state structs — documented in an "API for other modules" section. |

Rule for the whole phase: **with default settings everything behaves exactly like Phase 1**. This
was checked: the V1–V3 result files and the 20 × 12 demo numbers (68 changes, 48 handovers) are
byte-for-byte the same as before.

### P2.2 Step 11 — Several orbital planes (`NtnWalkerConstellation`)

**Idea.** One ring covers only a narrow belt under its ground track. Real constellations use P
rings with the same altitude and inclination, each turned by a different RAAN ("which way the
ring faces"), so together they cover the globe.

**The layout rule** (N satellites, P planes, S = N / P per plane). Satellite number k goes to plane
p = k ÷ S (integer division), slot j = k mod S, and gets

* RAAN = Raan0 + p · RaanSpread / P   (planes fanned out evenly)
* phase = phase0 + j · 360 / S + p · PlanePhaseOffset   (evenly spaced in the ring, each ring
  shifted a bit forward so neighbouring rings do not pass over the same spot at once)

RaanSpread = 360° is a *Walker delta* (inclined orbits), 180° a *Walker star* (polar orbits). In
the usual notation i:N/P/F the shift is PlanePhaseOffset = F · 360 / N.

**Worked example 1** (the unit test): 6 satellites, 3 planes, offset 20°, Raan0 = 10°,
phase0 = 5°. S = 2, so the ring spacing is 180° and the plane spacing 120°:

| k | plane | slot | RAAN | phase |
|---|---|---|---|---|
| 0 | 0 | 0 | 10° | 5° |
| 1 | 0 | 1 | 10° | 185° |
| 2 | 1 | 0 | 130° | 25° |
| 3 | 1 | 1 | 130° | 205° |
| 4 | 2 | 0 | 250° | 45° |
| 5 | 2 | 1 | 250° | 225° |

**Worked example 2** (the constellation example, 53°:120/10/1): S = 12 → 30° apart in a ring;
planes 36° apart; shift F·360/N = 3°. Satellite 27 → plane 2, slot 3 → RAAN 72°, phase
3·30 + 2·3 = 96°.

**Check by position:** a satellite with phase 0 sits on its *ascending node*, i.e. on the equator
at longitude = RAAN, whatever the inclination. So at t = 0 the first satellite of plane p must be
at (a·cos RAAN, a·sin RAAN, 0) — the test checks exactly that to 1 mm.

**Small but important detail.** To keep Phase 1 output identical, the in-ring spacing is computed
in the same order as before (`k * (360 / N)`, not `(k * 360) / N`) — the two can differ in the last
bit of a double.

120 satellites do not fit into 7 equal planes, so the helper stops with *"120 satellites cannot be
divided equally between 7 orbital planes; use a multiple of NumOrbitalPlanes"* instead of building
a strange constellation silently.

### P2.3 Step 12 — Several satellites per terminal (`MaxAssociationsPerGround`)

**Design.** The SRS still says "one active association", so each terminal keeps one **primary**
satellite (carries the default route; this is what `AssociationChange` and
`GetAssociatedSatellite()` report, exactly as in Phase 1) and up to K − 1 **secondary**
satellites whose links are kept up (useful e.g. for make-before-break handover or multipath in
other modules). A new trace `SecondaryAssociationChange(ground, satellite, added/removed)`
reports changes of the secondary set.

**Worked example** (test case): terminal at (0°, 0°), three equatorial satellites 2°, 8° and 10°
away (all within the 15.84° visibility limit at 600 km / 10°):

| Satellite | Earth-centre angle | Elevation | Range | Link delay |
|---|---|---|---|---|
| "near" | 2° | 67.79° | 643.5 km | 3.147 ms |
| "middle" | 8° | 28.75° | 1,106.5 km | 4.691 ms |
| "far" | 10° | 22.20° | 1,307.5 km | 5.361 ms |

With K = 2 the baseline policy ranks by elevation → primary = near, secondary = middle; far is
visible but not associated. Setting K back to 1 removes the secondary (one "removed" event) but
does **not** count as an association change of the terminal.

**Making the IP links follow.** The manager first stores the complete new set, *then* fires the
traces, so the IP link controller always sees the final state. A satellite that only moves from
primary to secondary (or back) is never taken down. Bringing an interface "up" twice is avoided,
because ns-3 would add its subnet route twice. The test runs 24 satellites × 4 terminals for 20
minutes with K = 3 and checks after **every** update that each IP link is up exactly when its pair
is associated.

### P2.4 Step 13 — A second policy (`StickyAssociationPolicy`) and the comparison (V4)

**Why.** The baseline switches as soon as another satellite climbs higher. Every handover costs
signalling and can lose a packet in flight. The sticky rule: *keep the current satellite as long
as it is visible*; when it is lost, promote the best secondary satellite (its link is already up
— make-before-break), otherwise take the highest one.

**Worked example** (scripted test): terminal at (0°, 0°), two equatorial satellites moving east
at ω = 360° / 5,792.33 s = 0.06215 °/s. A starts 2° past the terminal, B 8° before it.

| Time | A (angle, elevation) | B (angle, elevation) | Baseline uses | Sticky uses |
|---|---|---|---|---|
| 0 s | 2.00°, 67.8° | 8.00°, 28.7° | A | A |
| 80 s | 6.97°, 32.9° | 3.03°, 58.0° | **B** (handover) | A (A still visible) |
| 240 s | 16.92° → **not visible** | 6.92°, 33.2° | B | **B** (handover) |

Both make one handover here, but the sticky one happens later and only because A was lost.

**V4 — does it help in a real constellation?** Same scenario run twice: Walker 53°:120/10/1,
20 terminals between 45° S and 45° N, one hour, 1 s updates.

| Policy | Handovers | Coverage | Mean elevation | Mean delay |
|---|---|---|---|---|
| Highest elevation | 336 | 99.99 % | 30.93° | 4.927 ms |
| Sticky | 299 (−11 %) | 99.99 % | 24.91° | 5.479 ms |

So it is a **trade-off**: fewer handovers, but on average a lower (farther) satellite and about
0.55 ms more delay. At one altitude the highest satellite is always the nearest one, so the
baseline must have the lower mean delay — the validation checks that, too.

*Lesson learned:* the first V4 scenario used only 24 satellites. Both policies then made exactly
24 handovers — terminals were covered only 47 % of the time and almost never saw two satellites,
so there was nothing to choose. A comparison only makes sense where coverage overlaps.

### P2.5 Step 14 — Gateways (M01-FR-11)

**Mechanism.** With `EnableGateway = true`, gateways are registered like ground terminals
(`NtnHelper::InstallGateway(node, lat, lon)`). Each update first computes, for every
gateway–satellite pair, the same elevation / range / delay as for terminals, and each satellite
uses the visible gateway that sees it **highest** (fixed rule). New traces:
`GatewayVisibilityChange`, `GatewayLinkDelayUpdate`, `GatewayAssociationChange`.
`GetEndToEndPath(terminal)` gives terminal → primary satellite → its gateway with both delays.

**Worked example** (test): satellite straight above (0°, 0°); gateway G1 5° east, G2 10° west.

| Hop | Angle | Elevation | Range | Delay (incl. 1 ms) |
|---|---|---|---|---|
| terminal → satellite | 0° | 90° | 600.0 km | 3.0014 ms |
| satellite → G1 (chosen) | 5° | 43.35° | 835.5 km | 3.7868 ms |
| satellite → G2 (visible, not chosen) | 10° | 22.20° | 1,307.5 km | 5.3612 ms |

End-to-end delay = 3.0014 + 3.7868 = **6.7882 ms** — what the test expects to within 1 ns.
(Choosing the *lowest* gateway in a mutation check gives 8.3626 ms and fails three checks.)

**Real packets.** `InstallIpLinks()` also creates one point-to-point link per gateway–satellite
pair; every gateway owns the shared *gateway service address* 10.255.254.1. When a satellite
changes gateway, the controller (1) brings the old gateway link down, (2) brings the new one up,
(3) points the satellite's default route at the new gateway and (4) gives the new gateway a route
back to the terminals' link subnets through this satellite. Step (4) needs no "remove" partner:
reading the ns-3 source showed that `Ipv4StaticRouting` deletes every route through an interface
that goes down.

**Worked example** (integration test): 20 Mbit/s links, 200-byte UDP payload → 230 bytes on the
wire → 92 µs to transmit per hop. Gateway 6° east of the terminal, packet sent at t = 0.51 s:

| Part | Time |
|---|---|
| transmit on terminal link | 0.092 ms |
| terminal → satellite delay | 3.0014 ms |
| transmit on gateway link | 0.092 ms |
| satellite → gateway delay | 4.0598 ms |
| **arrival at gateway** | **0.51 s + 7.2452 ms = 0.5172452 s** |

The gateway echoes the packet; it comes back over the same two hops. Both arrival times match to
1 ns.

**Constellation example** (`ntn-example-constellation`, 120 satellites / 10 planes, 3 gateways,
20 terminals, 20 min): 23,980 echo requests sent, 22,039 reached a gateway (92 %), and every one
of those came back; mean one-way delay 9.55 ms (two hops). The 8 % lost are requests sent while
the terminal's satellite saw no gateway, or in flight during a handover — without
inter-satellite links (Phase 3) a satellite cannot pass traffic on to a neighbour that does see a
gateway.

### P2.6 Step 15 — API for other modules

`doc/futurenet-ntn.rst` now has an **"API for other modules"** section for the M02/M03 teams:
every query (`GetAssociatedSatellite(s)`, `GetVisibleSatellites`, `GetLinkState`,
`GetMobilityState`, `GetSatelliteGateway`, `GetEndToEndPath`, counters), every trace with its
signature, the ordering guarantee (gateway events before ground events; state already updated when
a trace fires), and how to plug in a new policy. Example for M02: listen to `AssociationChange` and
`GatewayAssociationChange` to update routes and read `GetEndToEndPath(g).totalDelay`.

### P2.7 Step 16 — Tests, mutation checks and problems found

Five new test cases (16 in total; the 11 Phase 1 cases are unchanged and pass):

| Test case | What it proves |
|---|---|
| Walker layout | defaults, rejected values, the angle table above, positions at t = 0 |
| MaxAssociationsPerGround | default 1 / range 1–4, ranking, traces, IP links = association set every update |
| Sticky policy | the rules, and the scripted two-satellite pass above |
| Gateway | gateway choice, 6.7882 ms end-to-end delay, traces and CSV/event-log rows |
| Gateway integration | real UDP packets terminal → satellite → gateway and back, exact arrival times |

Each feature was "mutated" on purpose to make sure a test notices: wrong plane offset, secondary
links not handled (12,032 inconsistent links), sticky policy ignoring its current satellite,
missing gateway return routes (0 of 3 replies), lowest-elevation gateway. All were caught.

Problems met on the way (and what we learned):

1. **A test expected 3 simultaneous satellites from a 12-satellite ring** — impossible, they are
   30° apart and the view window is ±15.8°. The test was wrong, not the code.
2. **A mutation crashed the test program (segmentation fault)** instead of failing. In ns-3's
   default mode a failed `NS_TEST_ASSERT` does *not* stop the test, so the next line read an
   element of an empty list. All new tests now check sizes before indexing and clean up the
   simulator before checking results.
3. **A test read a CSV file while it was still open** (the helper owning the writer was still
   alive). Fix: run the scenario in its own `{ }` block.
4. **One removed safety check was not detected**: keeping an old primary up when it becomes a
   secondary. Without the check the link just goes down and up again in the same instant and ends
   correct. The check stays (it avoids the pointless flap); this is written down honestly.

### P2.8 Phase 2 result in one table

| Item | Result |
|---|---|
| M01-FR-10 multiple planes + phase offsets | ✔ `NtnWalkerConstellation`, 1–20 planes |
| M01-FR-11 gateway association + path | ✔ traces, event-log rows, real IP path both ways |
| Second policy + comparison | ✔ sticky: −11 % handovers, +0.55 ms mean delay (V4) |
| `MaxAssociationsPerGround` 1–4 | ✔ default 1 = Phase 1 behaviour |
| API for other modules | ✔ section in `doc/futurenet-ntn.rst` (waiting for M02/M03 review) |
| Tests | ✔ 16 / 16 (11 Phase 1 unchanged) |
| Validation | ✔ V1–V4, 11 PASS lines; V1–V3 files identical to Phase 1 |
| Style / warnings | ✔ clang-format clean, 0 warnings |

Still not modelled after Phase 2: inter-satellite links (Phase 3), connections between gateways,
capacity limits, a pluggable gateway policy.

## Phase 3 — Optional requirements

*(will be added after Phase 3: inter-satellite links and any approved extras)*
