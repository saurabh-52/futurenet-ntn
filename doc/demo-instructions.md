# futurenet-ntn (M01) — Demonstration Instructions

Deliverable D9. Every command is run from the ns-3.46.1 root directory. With Docker, first open
a shell in the container: `docker exec -it ntn-dev bash` (see `README.md` for creating it).

## 0. One-command check (optional)

```bash
contrib/futurenet-ntn/tools/run-checks.sh
```

Runs configure, build, the test suite, the style check, both examples and all validation
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

Expected: `1 of 1 tests passed`, and 11 lines starting with `PASS` in the verbose run.

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

Expected: nine `PASS` lines and `ALL VALIDATION CASES PASSED`; V1 prints
`link delay 3.001385 ms (closed form 3.001385 ms)`.

## 7. Reproducibility

```bash
./ns3 run --cwd=demo "ntn-example-basic --linkStateFile=a.csv --RngSeed=3 --RngRun=7"
./ns3 run --cwd=demo "ntn-example-basic --linkStateFile=b.csv --RngSeed=3 --RngRun=7"
cmp demo/a.csv demo/b.csv && echo identical
cat demo/ntn-basic-run-info.txt     # ns-3 version, Git commit, seed, run, command line
```
