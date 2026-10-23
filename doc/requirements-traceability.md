# futurenet-ntn (M01) — Requirements Traceability

Status after **Phase 3** (Mandatory + Recommended + Optional ISLs). Test names refer to test cases in
`test/futurenet-ntn-test-suite.cc` (suite `futurenet-ntn`); validation cases are produced by
`examples/ntn-validation.cc` with results in `doc/validation/`.

## Functional requirements (SRS §6)

| ID | Pri | Requirement | Implementation | Verified by | Status |
|---|---|---|---|---|---|
| M01-FR-01 | M | Create/configure satellite and ground nodes | `NtnHelper::InstallSatellites`, `InstallGroundTerminal(s)`; `NtnLinkManager::AddSatelliteNode/AddGroundNode` | T5 reproducibility, T6 integration, all examples | Done |
| M01-FR-02 | M | Satellite position and velocity vs time | `CircularOrbitMobilityModel::GetPositionAt/GetVelocityAt`, `DoGetPosition/DoGetVelocity` | T3 M01-T01 position/velocity, T3 orbit period | Done |
| M01-FR-03 | M | Visibility with configurable minimum elevation | `NtnVisibilityModel` (`MinElevation`, `IsVisible`, `GetElevationDeg`) | T3 M01-T03 boundary, V3a | Done |
| M01-FR-04 | M | Slant range for every visible candidate | `NtnVisibilityModel::GetSlantRangeM`; `NtnLinkManager::GetVisibleSatellites`, `SlantRangeUpdate` trace | T3 M01-T05 (candidate list), T4 traces | Done |
| M01-FR-05 | M | Delay from path length / c + optional processing delay | `NtnPropagationDelayModel` (`PropagationSpeed`, `ProcessingDelay`) | T3 M01-T04 V1, V1, T6 | Done |
| M01-FR-06 | M | Association by policy; baseline highest elevation; one association per terminal | `NtnAssociationPolicy::Select`, `HighestElevationPolicy`; `NtnLinkManager::ApplyDecision` | T3 M01-T05, V3b | Done |
| M01-FR-07 | M | Re-evaluate every UpdateInterval | `NtnLinkManager::Start/PeriodicUpdate/Update`, Attribute `UpdateInterval` | T3 M01-T03 (moving), T4 | Done |
| M01-FR-08 | M | TraceSources for visibility/association changes | `VisibilityChange`, `AssociationChange`, `SlantRangeUpdate`, `LinkDelayUpdate` (+ `UpdateCompleted`) | T4 trace sources | Done |
| M01-FR-09 | M | Configurable logical link DataRate | `NtnLinkManager` Attribute `DataRate`; applied to p2p devices by `NtnHelper::InstallIpLinks` | T1, T2, T6 (arrival time includes tx time at 50 Mbit/s) | Done |
| M01-FR-10 | R | Multiple orbital planes, phase offsets | `NtnWalkerConstellation` (`NumOrbitalPlanes`, `PlanePhaseOffset`, `RaanSpread`); `NtnHelper::SetConstellationAttribute`, `InstallSatellites`, `GetOrbitSlot` | `T1 T2 T3 M01-FR-10 ...`; V4 scenario; `ntn-example-constellation` | Done |
| M01-FR-11 | R | Gateway node and satellite-gateway association | `NtnLinkManager` `EnableGateway`, `AddGatewayNode`, `UpdateGateways`, `GetSatelliteGateway`, `GetEndToEndPath`; traces `GatewayVisibilityChange`/`GatewayAssociationChange`/`GatewayLinkDelayUpdate`; `NtnIpLinkController` gateway links + routes; `NtnHelper::InstallGateway` | `T1 T3 T4 M01-FR-11 ...`, `T6 M01-FR-11 ...`; `ntn-example-constellation` | Done |
| (R) policy abstraction | R | Selectable policy + second policy, handover comparison | `NtnHelper::SetPolicy`; `StickyAssociationPolicy` | `T3 StickyAssociationPolicy ...`; V4 | Done |
| (R) MaxAssociationsPerGround | R | 1–4 simultaneous associations, default 1 | `NtnLinkManager` Attribute; primary + secondary decision; `SecondaryAssociationChange` trace; IP controller | `T1 T2 T3 T4 MaxAssociationsPerGround ...` | Done |
| (R) integration readiness | R | Other modules use only public API, traces, common structs | `doc/futurenet-ntn.rst` → "API for other modules"; `NtnEndToEndPath` | review by M02/M03 teams | Done (review pending) |
| M01-FR-12 | O | Inter-satellite links | `NtnIslModel` (`MaxIslRange`, `IslMinClearance`, `ComputeLineClearanceM`); `NtnLinkManager` `EnableIsl`, `UpdateIsls`, `GetIslState(s)`, `GetIslNeighbours`, `GetIslLinkState`; traces `IslStateChange`, `IslDelayUpdate`; event-log rows + ISL CSV | `T1 T2 T3 T4 M01-FR-12 ...`; V5; `ntn-example-isl` | Done (link state; routing over ISLs left to M02) |
| (O) extras | O | Earth rotation, link budget, energy counter | — | — | Not done (not selected by the user) |

## Tests (SRS §11) and Guideline test categories (§9)

| ID | Description | Test case | Status |
|---|---|---|---|
| T1 (guideline) | Construction / defaults | `T1 construction and SRS default values` | Done |
| M01-T01 | Position at known time deterministic | `T3 M01-T01 deterministic satellite position and velocity` | Done |
| M01-T02 | Invalid altitude/elevation rejected | `T2 M01-T02 invalid attribute values are rejected` (also UpdateInterval, DataRate, ProcessingDelay, PropagationSpeed, GroundHeight, lat/lon) | Done |
| M01-T03 | Visibility changes at boundary | `T3 M01-T03 visibility changes at the elevation boundary` | Done |
| M01-T04 | Delay = path / speed within tolerance | `T3 M01-T04 V1 propagation delay equals path length over c` (1 ns) | Done |
| M01-T05 | Policy selects expected satellite | `T3 M01-T05 highest-elevation policy picks the expected satellite` | Done |
| M01-T06 | Fixed seed/run repeatable | `T5 M01-T06 repeated runs give identical CSV output` (byte-identical) | Done |
| T4 (guideline) | TraceSource / state transitions | `T4 M01-FR-08 trace sources fire with correct arguments` | Done |
| T6 (guideline) | Integration with standard ns-3 models | `T6 M01-FR-09 packets over IP see the computed delay and data rate` | Done |
| (prompt) | Orbit period 600 km ≈ 96.5 min | `T3 orbit period at 600 km is 5792.3 s (96.5 min)` | Done (see deviation in technical report) |
| FR-10 (P2) | Walker layout defaults, range, angles, positions | `T1 T2 T3 M01-FR-10 multi-plane Walker constellation layout` | Done |
| MaxAssoc (P2) | Default/range, ranking, traces, IP links = association set | `T1 T2 T3 T4 MaxAssociationsPerGround keeps up to 4 satellites associated` | Done |
| Policy (P2) | Sticky rules, scripted pass vs baseline | `T3 StickyAssociationPolicy stays until the satellite is lost` | Done |
| FR-11 (P2) | Gateway choice, end-to-end delay, traces, CSV/event rows | `T1 T3 T4 M01-FR-11 gateway association, end-to-end path and traces` | Done |
| FR-11 (P2) | Real packets ground → satellite → gateway and back | `T6 M01-FR-11 packets go ground -> satellite -> gateway and back` | Done |
| FR-12 (P3) | ISL defaults/range, clearance geometry, manager state, traces, CSV/event rows | `T1 T2 T3 T4 M01-FR-12 inter-satellite links: range and Earth blocking` | Done |

## Validation (SRS §12, Guidelines §10)

| ID | Description | Where | Result | Status |
|---|---|---|---|---|
| V1 | Static known distance: overhead 600 km → 2.0014 ms + processing | `ntn-validation` V1 → `doc/validation/v1-overhead-delay.csv`; test `T3 M01-T04 V1` | error 0.43 ns | Done |
| V2 (guideline) | One parameter varied: altitude 200–2000 km | `v2-altitude-sweep.csv` | increasing, equals h/c | Done |
| V2 (SRS) | One pass: range decreases then increases | `v2-single-pass.csv`; test `V2 V3 AC-05 ...` | PASS | Done |
| V3 (guideline) | Boundary: MinElevation 0/45°, altitude 200/2000 km | `v3-boundary.csv` | all within one update interval of closed form | Done |
| V3 (SRS) | Association only to visible satellites | `v3-association-visible.csv`; test `V2 V3 AC-05 ...` | 0 of 68 changes invalid | Done |
| V4 (Phase 2) | Policy comparison: handovers, delay, coverage | `v4-policy-comparison.csv` | sticky 299 vs baseline 336 handovers; delay 5.48 vs 4.93 ms; equal coverage | Done |
| V5 (Phase 3) | ISL geometry vs closed form; only the Earth limits ISLs | `v5-isl.csv` | ring distance exact, delay error 0.31 ns; 0 inconsistent; longest 5,658.68 vs limit 5,658.69 km | Done |

## Acceptance criteria (SRS §16)

| ID | Criterion | Evidence | Status |
|---|---|---|---|
| AC-01 | Builds cleanly with ns-3.46.1 | Docker image ns-3.46.1 (GCC 13.3); `./ns3 build` with zero module warnings | Done |
| AC-02 | All M requirements implemented | FR-01 … FR-09 above | Done |
| AC-03 | Automated tests pass | `./test.py -s futurenet-ntn`: 17/17 test cases PASS (11 Phase 1 unchanged + 5 Phase 2 + 1 Phase 3) | Done |
| AC-04 | Delay validation numerically correct | V1 (0.43 ns error), T3 M01-T04 (1 ns tolerance), T6 | Done |
| AC-05 | Handover example shows ≥ 1 association change | `ntn-example-advanced --numGround=20 --numSats=12`: 68 changes, 48 handovers; test `V2 V3 AC-05` | Done |
| AC-06 | Parameters configurable via Attributes/helper | Attributes table in `doc/futurenet-ntn.rst`; `NtnHelper::Set*Attribute`; example command-line options | Done |
| AC-07 | Documentation states abstraction limitations | `doc/futurenet-ntn.rst` "Scope and Limitations", README, technical report §4 | Done |

## Quality gate (Common Guidelines §18)

| ID | Gate | Evidence | Status |
|---|---|---|---|
| QG-01 | Clean build on agreed ns-3.46.1 environment | `tools/docker/Dockerfile`; clean build of `main` from a fresh clone | Done |
| QG-02 | All Mandatory tests pass | AC-03 | Done |
| QG-03 | Basic example runs without modification | `ntn-example-basic` (also run by `tools/run-checks.sh`) | Done |
| QG-04 | Parameters configurable; no experiment hard-coding | All SRS parameters are Attributes/helper inputs; constants are named (`model/ntn-constants.h`) | Done |
| QG-05 | Machine-readable traces | Link-state CSV + common event log CSV + FlowMonitor XML | Done |
| QG-06 | Public API documented | Doxygen comments on all public classes/methods; `doc/futurenet-ntn.rst` | Done |
| QG-07 | No ns-3 core modification | All code under `contrib/futurenet-ntn`; nothing in `src/` changed | Done |
| QG-08 | No name collisions | New types prefixed `Ntn` (SRS-named `CircularOrbitMobilityModel`, `HighestElevationPolicy` kept); TypeIds checked unique at registration; log components prefixed | Done |
| QG-09 | ≥ 3 validation cases | V1, V2, V3 (+ SRS V2, V3), V4 (Phase 2), V5 (Phase 3) | Done |
| QG-10 | Limitations documented | AC-07 | Done |

## Deliverables (Guidelines §17)

| ID | Deliverable | Location | Status |
|---|---|---|---|
| D1 | Module source | `model/`, `helper/` | Done |
| D2 | CMake integration | `CMakeLists.txt`, `examples/CMakeLists.txt` | Done |
| D3 | Tests | `test/futurenet-ntn-test-suite.cc` | Done |
| D4 | ≥ 2 examples | `ntn-example-basic`, `ntn-example-advanced`, `ntn-example-constellation`, `ntn-example-isl` (+ `ntn-validation`) | Done |
| D5 | README + technical docs | `README.md`, `doc/futurenet-ntn.rst`, `setup_guide.md`, `doc/project-walkthrough.md` | Done |
| D6 | Parameter table | `doc/futurenet-ntn.rst` → Attributes | Done |
| D7 | Validation results + raw output | `doc/validation/` | Done |
| D8 | Technical report | `doc/technical-report.md` | Done |
| D9 | Demo instructions | `doc/demo-instructions.md` | Done |
| D10 | Git history | this repository (`main`, `develop`, `feature/*`) | Done |
| opt. | Python analysis script | `examples/plot_ntn.py` | Done |
