# futurenet-ntn (M01) — Demonstration Instructions

Deliverable D9. Every command is run from the ns-3.46.1 root directory. With Docker, first open
a shell in the container: `docker exec -it ntn-dev bash` (see `README.md` for creating it).

## 0. One-command check (optional)

```bash
contrib/futurenet-ntn/tools/run-checks.sh
```

Runs configure, build, the test suite, the style check, all three examples and all validation
cases. Expected last line: `All checks finished. Example outputs are in ntn-output/`.

## 1. Build

```bash
./ns3 configure --enable-examples --enable-tests \
  --enable-modules=core,network,mobility,internet,point-to-point,applications,flow-monitor,futurenet-ntn
./ns3 build
```

Expected: build finishes without errors or warnings from `contrib/futurenet-ntn`.

## 2. Automated tests (AC-03)

```bash
./test.py -s futurenet-ntn
./build/utils/ns3.46.1-test-runner-default --test-name=futurenet-ntn --verbose   # per test case
```

Expected: `1 of 1 tests passed`, and 17 test-case lines starting with `PASS` in the verbose run
(11 Phase 1 + 6 Phase 2).

## 3. Basic example — one satellite pass

```bash
mkdir -p demo && ./ns3 run --cwd=demo ntn-example-basic
```

Expected (excerpt): the satellite rises at ~228 s, is closest near 482 s, sets at ~738 s:

```text
Orbit altitude 600 km, period +5792.33s (96.5389 min)
  time_s  elev_deg   range_km  delay_ms  visible  associated
   200.0       7.4     2122.2     8.079       no          no
  [+227.900s] ground 1 association -1 -> 0
   250.0      12.3     1781.7     6.943      yes         yes
   450.0      67.5      644.9     3.151      yes         yes
   500.0      77.7      612.9     3.044      yes         yes
  [+737.500s] ground 1 association 0 -> -1
Association changes of ground node 1: 2
```

Show `demo/ntn-basic-events.csv` (common event log):

```text
time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail
227.900000,futurenet-ntn,VisibilityChange,1,0,,10.000505,1931.600175,elevation_deg|range_km,visible
227.900000,futurenet-ntn,AssociationChange,1,0,,-1,7.443125,old_satellite_id|delay_ms,acquired
737.500000,futurenet-ntn,VisibilityChange,1,0,,9.999432,1931.674916,elevation_deg|range_km,not_visible
737.500000,futurenet-ntn,AssociationChange,1,-1,,0,,old_satellite_id|delay_ms,lost
```

## 4. Advanced example — 20 ground terminals × 12 satellites with traffic (AC-05, SRS §13)

```bash
./ns3 run --cwd=demo "ntn-example-advanced --numGround=20 --numSats=12"
```

Expected (excerpt): every terminal has 2–3 handovers in 20 minutes and almost all UDP packets
arrive with a mean one-way delay of ~4.5–5 ms:

```text
ground  assoc_changes  handovers  tx_pkts  rx_pkts  mean_delay_ms
    12              3          2     1199     1199          4.809
    13              4          3     1199     1198          4.878
...
Total association changes: 68, total handovers: 48
```

(A missing packet is one that was in flight on the old link at a handover — see limitations.)

Show the configurability (AC-06) by changing parameters without recompiling, e.g.:

```bash
./ns3 run --cwd=demo "ntn-example-advanced --numGround=5 --numSats=6 --minElevationDeg=25 --altitudeKm=1200 --dataRate=10Mbps"
./ns3 run "ntn-example-advanced --PrintHelp"
```

Show rejection of an invalid value (AC-06 / M01-T02):

```bash
./ns3 run "ntn-example-basic --altitudeKm=100"
# -> stops with: msg="Invalid value for attribute set (OrbitAltitude) on ns3::CircularOrbitMobilityModel"
```

## 4b. Constellation example — Walker planes + gateways (Phase 2)

```bash
./ns3 run --cwd=demo ntn-example-constellation
```

Expected (about 12 s):

```text
Constellation: 120 satellites in 10 planes (12 per plane), 3 gateways, 20 terminals, policy highest, up to 1 association(s) per terminal
Terminal association changes: 124, handovers: 104, satellite gateway changes: 32
Uplink   (terminal -> gateway): 23980 sent, 22039 received, mean one-way delay 9.550 ms
Downlink (gateway -> terminal): 22039 sent, 22039 received, mean one-way delay 9.548 ms
```

Explain: each echo request goes terminal → satellite → gateway (two hops of ~4–5 ms incl. 1 ms
processing each); ~8 % of requests are lost while the terminal's satellite has no gateway in view
or during a handover (no inter-satellite links yet). Every echo that reaches a gateway comes back.

Show the Phase 2 options without recompiling:

```bash
./ns3 run --cwd=demo "ntn-example-constellation --policy=sticky --maxAssociations=2"
./ns3 run --cwd=demo "ntn-example-constellation --numSats=60 --numPlanes=6 --numGateways=2"
./ns3 run --cwd=demo "ntn-example-constellation --numPlanes=7"
# -> stops with: "120 satellites cannot be divided equally between 7 orbital planes ..."
grep -m3 Gateway demo/ntn-constellation-events.csv    # gateway rows in the common event log
```

## 5. Plot (optional)

```bash
python3 contrib/futurenet-ntn/examples/plot_ntn.py demo/ntn-advanced-link-state.csv --ground 12 --output demo/ntn-plot.png
```

Shows slant range per visible satellite, link delay of the active link, and the associated
satellite over time (handovers are the steps).

## 6. Validation (AC-04)

```bash
./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"
```

Expected: eleven `PASS` lines and `ALL VALIDATION CASES PASSED`; V1 prints
`link delay 3.001385 ms (closed form 3.001385 ms)`, V4 prints

```text
V4 ns3::HighestElevationPolicy: 336 handovers, mean elevation 30.93 deg, mean delay 4.927 ms
V4 ns3::StickyAssociationPolicy: 299 handovers, mean elevation 24.91 deg, mean delay 5.479 ms
```

## 7. Reproducibility

```bash
./ns3 run --cwd=demo "ntn-example-basic --linkStateFile=a.csv --RngSeed=3 --RngRun=7"
./ns3 run --cwd=demo "ntn-example-basic --linkStateFile=b.csv --RngSeed=3 --RngRun=7"
cmp demo/a.csv demo/b.csv && echo identical
cat demo/ntn-basic-run-info.txt     # ns-3 version, Git commit, seed, run, command line
```
