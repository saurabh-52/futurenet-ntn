# futurenet-ntn — NTN Connectivity and LEO Satellite Network Model (M01)

A reusable ns-3.46.1 contrib module that models simplified non-terrestrial network (NTN)
connectivity: LEO satellites moving on circular orbits, ground terminals, satellite visibility
from an elevation threshold, slant range, distance-based propagation delay, one active
satellite association per ground terminal, handovers, trace sources, and real IP traffic over
point-to-point links whose delay follows the geometry.

> This is a **geometry / distance / delay abstraction for teaching and system-level studies**.
> It is **not** a 3GPP NR-NTN PHY/MAC model and makes **no claim of 3GPP or other standards
> conformance**. Parameter ranges and defaults are project configuration values (SRS M01 §7),
> not standardised values.

## Dependencies

| Kind | Modules |
|---|---|
| Required (library) | `core`, `mobility`, `network`, `internet`, `point-to-point` |
| Examples only | `applications`, `flow-monitor` |
| Optional | none (`futurenet-common` was not supplied; see `model/ntn-common-types.h`) |

This module does not depend on M02 or any other FutureNet module.

## Build

The module lives in `contrib/futurenet-ntn/` of an ns-3.46.1 tree.

```bash
cd ns-3.46.1
./ns3 configure --enable-examples --enable-tests
./ns3 build
./test.py -s futurenet-ntn
```

Faster build that compiles only what this module needs:

```bash
./ns3 configure --enable-examples --enable-tests \
  --enable-modules=core,network,mobility,internet,point-to-point,applications,flow-monitor,futurenet-ntn
./ns3 build
```

### Using the Docker environment (Windows / any OS)

`tools/docker/Dockerfile` builds Ubuntu 24.04 + ns-3.46.1 + clang-format 20:

```bash
docker build -t futurenet-ns3:3.46.1 tools/docker
docker run -d --name ntn-dev \
  -v "<path-to>/futurenet-ntn:/opt/ns-allinone-3.46.1/ns-3.46.1/contrib/futurenet-ntn" \
  -w /opt/ns-allinone-3.46.1/ns-3.46.1 futurenet-ns3:3.46.1 sleep infinity
docker exec ntn-dev contrib/futurenet-ntn/tools/run-checks.sh   # configure, build, test, style, examples, validation
```

## Basic usage

```cpp
#include "ns3/futurenet-ntn-module.h"
#include "ns3/internet-module.h"

NodeContainer satellites, grounds;
satellites.Create(12);
grounds.Create(20);

NtnHelper ntn;
ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(600e3));      // metres
ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(53.0));    // degrees
ntn.SetVisibilityAttribute("MinElevation", DoubleValue(10.0));   // degrees
ntn.SetLinkAttribute("UpdateInterval", TimeValue(MilliSeconds(100)));
ntn.SetLinkAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
ntn.InstallSatellites(satellites);                               // spread evenly on one ring
ntn.InstallGroundTerminal(grounds.Get(0), 48.1, 11.6);           // latitude, longitude [deg]

InternetStackHelper().Install(satellites);
InternetStackHelper().Install(grounds);
Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
ntn.InstallIpLinks(addresses);                                   // one p2p link per pair

ntn.EnableTracing("link-state.csv", "events.csv", Seconds(1));
ntn.Start();
// Send traffic to ntn.GetSatelliteServiceAddress(): the associated satellite receives it.
```

## Examples

| Program | What it shows |
|---|---|
| `ntn-example-basic` | 1 ground station + 1 satellite pass: range/delay table, link-state CSV, event log |
| `ntn-example-advanced` | N terminals + ring of satellites, UDP traffic, FlowMonitor, handover counts |
| `ntn-validation` | Validation cases V1–V3 with closed-form comparison, CSVs in `doc/validation/` |
| `examples/plot_ntn.py` | Optional matplotlib plot of range, delay and association from the CSV |

```bash
./ns3 run "ntn-example-basic --simulationTime=1000s"
./ns3 run "ntn-example-advanced --numGround=20 --numSats=12"     # SRS demonstration size
./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"
python3 contrib/futurenet-ntn/examples/plot_ntn.py ntn-advanced-link-state.csv --ground 12
```

All examples accept `--RngSeed` / `--RngRun` and write a `*-run-info.txt` file (ns-3 version,
Git commit, seed, run, command line). Use `--PrintHelp` to list every option.

## Output files

* **Link-state CSV** (SRS §9):
  `time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated`
* **Common event log** (Common Guidelines §4.5):
  `time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail`

## Known limitations (summary — full list in `doc/futurenet-ntn.rst`)

* Spherical Earth (R = 6,371 km); no Earth rotation; circular orbits only; no orbital
  perturbations.
* Visibility is purely geometric (elevation ≥ MinElevation); no obstruction, weather, antenna,
  beam, or link-budget model.
* Handover is break-before-make and instantaneous; a packet in flight on the old link at the
  handover instant is dropped.
* No PHY/MAC, no Doppler, no NR numerology, no regenerative payload, no standards conformance.

## Documentation

* `doc/futurenet-ntn.rst` — model description, formulas, parameters, traces, validation,
  extension points, limitations
* `doc/technical-report.md` — design decisions and limitations (D8)
* `doc/demo-instructions.md` — step-by-step demo (D9)
* `doc/requirements-traceability.md` — SRS / quality-gate traceability
