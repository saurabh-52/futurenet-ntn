# futurenet-ntn (M01) — Technical Report: Design Decisions and Limitations

Deliverable D8. Phase 1 (Mandatory requirements). ns-3.46.1.

## 1. Goal

Provide a reusable ns-3 module that tells, at any simulation time, which LEO satellites a
ground terminal can see, how far away they are, what one-way delay a link to them has, and which
satellite the terminal is associated with — and that carries real IP packets with that delay.
It is a system-level geometry/delay abstraction, not a 3GPP NR-NTN model, and claims no
standards conformance.

## 2. Key design decisions

| # | Decision | Reason |
|---|---|---|
| 1 | Earth-centred Cartesian frame in metres, spherical Earth R = 6,371 km, fixed constant | Simple, exact formulas; same sphere as ns-3 `GeographicPositions`, so ground conversion is reused (CG-15). SRS §7 allows a fixed EarthRadius. |
| 2 | Earth rotation ignored | Keeps ground terminals static and orbits analytic; acceptable for pass-scale studies; documented limitation. |
| 3 | Orbit period from T = 2π√(a³/μ) (SRS §7 option a) | Physically consistent; no separate period parameter that could contradict the altitude. |
| 4 | `GetPositionAt(t)` is a pure function of time | Exact reproducibility (M01-T01, M01-T06); no accumulated numerical error. |
| 5 | Satellite placement = in-plane circle → tilt by inclination (x) → turn by RAAN (z) | Standard construction; RAAN already in place for multiple planes (Phase 2). |
| 6 | `OrbitAltitude` Attribute in metres | Guidelines §5 (SI units); resolves SRS km-vs-metres inconsistency; examples take `--altitudeKm`. |
| 7 | Separate classes for visibility, delay, policy, manager, IP controller, trace writer | One responsibility each (CG-05); mechanism/policy split (§1.3, §4.6); delay model reusable for gateway and ISL links later. |
| 8 | `NtnAssociationPolicy::Select(const NtnAssociationState&)` | State holds all visible candidates (elevation, range, delay) + current association, so an RL/optimisation policy can replace the baseline without touching the manager. |
| 9 | Manager validates the policy decision (must be visible or -1) | Guarantees SRS V3 for any future policy. |
| 10 | One PointToPoint link per ground–satellite pair, channel `Delay` updated, IPv4 interfaces up only while associated, default route moved on handover | SRS §3 allows an abstract IP link; reuses ns-3 PointToPoint/Internet (CG-15); simplest approach that carries real packets with the changing delay. |
| 11 | Shared "satellite service address" on every satellite's loopback | Lets applications send to "my current satellite" without reconfiguration at handover. One-way flows are used because echo replies would come from a different source address. |
| 12 | IP actuation (`NtnIpLinkController`) driven by traces, not called by the manager | Manager stays network-agnostic, so M02 can reuse it with another network setup. |
| 13 | Bounded Attribute checkers + `NS_ABORT_MSG_IF` in setters | CG-11: invalid values fail loudly; `SetAttributeFailSafe` makes rejection testable. A custom bounded `DataRate` checker was added because ns-3 has none. |
| 14 | `ntn-common-types.h` mirrors the common `LinkState`/`MobilityState` contract | `futurenet-common` was not supplied; same field names/units keep the integration contract. |
| 15 | Deterministic iteration (registration order, `std::map`), fixed CSV number formatting, no random variables | CG-09/CG-10: byte-identical output for identical configuration (test T5). |
| 16 | Link-state CSV sampling interval | Writing every pair every 100 ms would be ~150 MB for 20 × 12 over 20 min; SRS §9 allows sampling. Events are never sampled. |
| 17 | Run-info file per example run (version, commit, seed, run, command line) | Guidelines §8.3. Commit is captured at configure time (`tools/run-checks.sh` re-configures first). |

## 3. Verification summary

* 11 automated test cases (T1–T6 categories), all passing; a mutation check (adding 5 ns to the
  delay) makes the suite fail, confirming the 1 ns assertions are effective.
* Validation V1–V3 against closed form: overhead delay error 0.43 ns; boundary visibility
  durations within one update interval; 0 associations to non-visible satellites.
* SRS demonstration size (20 ground × 12 satellites, 20 min, UDP traffic) runs in ~3 s with
  48 handovers.
* clang-format check (ns-3 `utils/check-style-clang-format.py`, clang-format 20) clean; zero
  compiler warnings from the module.

## 4. Limitations

See `doc/futurenet-ntn.rst`, "Scope and Limitations". Most important for users:

1. Spherical, non-rotating Earth; circular orbits only.
2. Purely geometric visibility; no link budget, fading, weather, antenna or beam model; zero link
   loss.
3. No PHY/MAC, Doppler, timing advance or NR numerology; no standards conformance.
4. Break-before-make, instantaneous handover; in-flight packets on the old link are dropped.
5. Decisions are quantised to `UpdateInterval`.
6. Phase 1: single orbit plane; no gateway, no ISLs (planned for Phases 2 and 3).

## 5. Deviations from the SRS / prompt

| Item | Deviation | Reason |
|---|---|---|
| Orbit period check | Test uses 5,792.33 s ± 1 s instead of "≈ 5,790 s ± 1 s" | The exact formula with R = 6,371 km and the given μ yields 5,792.33 s (96.54 min); 5,790 is a rounded figure that would fail a ±1 s test. |
| `OrbitAltitude` unit | metres, not km | SI units (Guidelines §5); matches SRS method `SetOrbitAltitude(double meters)`. |
| `NumSatellites` | container size in `InstallSatellites`, validated 1–200, not an Attribute | In ns-3 the user creates the nodes; a second count could contradict it. |
| `EarthRadius` | fixed constant | SRS §7 permits "fixed or configurable"; enables reuse of ns-3 geographic conversion. |
| `DoSetPosition` | not supported (aborts with message) | Position is a function of orbit parameters/time or lat/lon/height. |
| Class names `CircularOrbitMobilityModel`, `HighestElevationPolicy` | kept without the `Ntn` prefix | Names are prescribed by the SRS; all other new types are `Ntn`-prefixed (QG-08). |
| clang-format version | 20 installed from PyPI | ns-3.46.1's style script accepts only clang-format 20; Ubuntu 24.04 ships 18. |
