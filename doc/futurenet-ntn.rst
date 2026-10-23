FutureNet NTN: LEO Satellite Connectivity Model (M01)
-------------------------------------------------------

.. highlight:: cpp

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

This chapter describes the ``futurenet-ntn`` contrib module for ns-3.46.1 (FutureNet project
M01). The module represents simplified non-terrestrial-network (NTN) connectivity: LEO
satellites that move on circular orbits, ground terminals, geometric visibility, slant range,
distance-derived propagation delay, satellite association and handover, and real IP traffic over
links whose delay follows the geometry.

.. important::

   The model is a **geometry / distance / delay abstraction**. It is not a 3GPP NR-NTN PHY/MAC
   implementation and claims **no conformance** to 3GPP TR 38.811, TR 38.821 or any other
   standard. All parameter ranges and defaults are *project configuration values* chosen for
   teaching and testing (SRS M01 section 7, Common Guidelines section 5).

Model Description
*****************

The source code lives in ``contrib/futurenet-ntn``: ``model/`` (reusable classes),
``helper/`` (``NtnHelper``), ``examples/``, ``test/`` and ``doc/``.

Design
======

Architecture
############

The module separates *mechanism* (computing geometry, delays and applying decisions) from
*policy* (choosing a satellite), following Common Guidelines sections 1.3 and 4.6.

.. code-block:: text

      Satellite node --- CircularOrbitMobilityModel        Ground node --- NtnGroundTerminalMobilityModel
                |            (position = f(time))                |          (lat / lon / height)
                +---------------------+    +---------------------+
                                      v    v
                               +-----------------+   uses   +---------------------------+
                               | NtnLinkManager  |--------->| NtnVisibilityModel        |
                               |  (mechanism,    |   uses   | NtnPropagationDelayModel  |
                               |   every Update- |--------->+---------------------------+
                               |   Interval)     |   asks   +---------------------------+
                               |                 |--------->| NtnAssociationPolicy      |
                               +-----------------+          |  <- HighestElevationPolicy|
                                 | TraceSources             +---------------------------+
               +-----------------+------------------+
               v                                    v
     +---------------------+              +------------------+
     | NtnIpLinkController |              | NtnTraceWriter   |
     | (IPv4 up/down,      |              | (link-state CSV, |
     |  p2p channel Delay, |              |  common event    |
     |  default route)     |              |  log)            |
     +---------------------+              +------------------+

            NtnHelper creates and wires all of the above with explicit calls.

Class relationships (Mermaid source, renders on GitHub/GitLab):

.. code-block:: text

   classDiagram
     MobilityModel <|-- CircularOrbitMobilityModel
     MobilityModel <|-- NtnGroundTerminalMobilityModel
     Object <|-- NtnVisibilityModel
     Object <|-- NtnPropagationDelayModel
     Object <|-- NtnAssociationPolicy
     NtnAssociationPolicy <|-- HighestElevationPolicy
     Object <|-- NtnLinkManager
     Object <|-- NtnIpLinkController
     Object <|-- NtnTraceWriter
     NtnLinkManager o-- NtnVisibilityModel
     NtnLinkManager o-- NtnPropagationDelayModel
     NtnLinkManager o-- NtnAssociationPolicy
     NtnIpLinkController ..> NtnLinkManager : listens to traces
     NtnTraceWriter ..> NtnLinkManager : listens to traces
     NtnHelper ..> NtnLinkManager : creates

Responsibilities (one per class, CG-05):

* ``CircularOrbitMobilityModel`` -- satellite position/velocity as a pure function of time.
* ``NtnGroundTerminalMobilityModel`` -- fixed terminal placed by latitude/longitude/height.
* ``NtnVisibilityModel`` -- elevation, slant range and the ``MinElevation`` visibility rule.
* ``NtnPropagationDelayModel`` -- path length to delay (``PropagationSpeed``,
  ``ProcessingDelay``).
* ``NtnAssociationPolicy`` / ``HighestElevationPolicy`` -- choose one satellite per terminal.
* ``NtnLinkManager`` -- periodic update loop, per-pair state, trace sources, state queries.
* ``NtnIpLinkController`` -- applies association and delay to real IPv4 point-to-point links.
* ``NtnTraceWriter`` -- machine-readable CSV output.
* ``NtnHelper`` -- installation and configuration.

Coordinate system and Earth model
#################################

All positions are **Earth-centred Cartesian coordinates in metres**: origin at the centre of the
Earth, z axis through the North Pole, x axis through latitude 0 / longitude 0. The Earth is a
**sphere** of radius ``EarthRadius = 6,371,000 m`` (a fixed constant, ``ntn::EARTH_RADIUS_M``;
SRS section 7 allows "fixed"). This equals ``GeographicPositions::EARTH_SPHERE_RADIUS``, so ground
terminals are converted with ns-3's own
``GeographicPositions::GeographicToCartesianCoordinates(..., SPHERE)`` (CG-15).

**Earth rotation is ignored**: ground terminals do not move in this frame and satellite ground
tracks are fixed great circles.

Angles are configured, logged and written to CSV in **degrees**; the code converts them to
**radians** internally (``ntn::DegToRad``/``ntn::RadToDeg``). The orbit altitude is configured
in **metres** (the SRS table lists km while the SRS method takes metres; metres was chosen per
Common Guidelines section 5 -- examples accept ``--altitudeKm`` and convert).

Orbit model
###########

A satellite moves on a circle of radius :math:`a = R + h` (R = Earth radius, h =
``OrbitAltitude``).

*Orbit period* -- SRS section 7 option (a), the circular-orbit equation. Gravity must supply
exactly the centripetal force, which gives

.. math::

   T = 2\pi\sqrt{\frac{a^3}{\mu}}, \qquad \omega = \frac{2\pi}{T} = \sqrt{\frac{\mu}{a^3}},
   \qquad \mu = 3.986004418\times10^{14}\ \mathrm{m^3/s^2}

At 600 km: T = 5,792.33 s (96.54 min), speed :math:`a\omega` = 7,561.7 m/s.

*Position* -- with phase :math:`u(t) = u_0 + \omega t` (``InitialPhase`` :math:`u_0`), the
satellite is first placed in its orbit plane at :math:`(a\cos u, a\sin u, 0)`, then the plane
is tilted by the inclination *i* (rotation about x) and turned by the RAAN :math:`\Omega`
(rotation about z, "which way the ring faces"):

.. math::

   x &= a(\cos u\cos\Omega - \sin u\cos i\sin\Omega)\\
   y &= a(\cos u\sin\Omega + \sin u\cos i\cos\Omega)\\
   z &= a\,\sin u\sin i

The velocity is the time derivative (replace :math:`\cos u \to -\omega\sin u`,
:math:`\sin u \to \omega\cos u`). ``GetPositionAt(t)`` is a pure function of time -- no state
is kept -- which makes positions exactly reproducible. ``SetPosition()`` is **not supported** and
stops the simulation with a message, because the position follows from the orbit parameters.

Visibility, slant range and delay
#################################

For ground position **g** and satellite position **s**, the line of sight is
:math:`\mathbf{d} = \mathbf{s} - \mathbf{g}`, and on a spherical Earth "straight up" at the
terminal is :math:`\mathbf{g}/|\mathbf{g}|`. Then

.. math::

   \text{slant range} = |\mathbf{d}|, \qquad
   \sin(\text{elevation}) = \frac{\mathbf{d}\cdot\mathbf{g}/|\mathbf{g}|}{|\mathbf{d}|}

i.e. the elevation is the angle of the line of sight above the local horizon. A satellite is
**visible** when ``elevation >= MinElevation`` (a satellite exactly at the threshold is
visible). Then

.. math::

   \text{propagation delay} = \frac{\text{slant range}}{\text{PropagationSpeed}}, \qquad
   \text{link delay} = \text{propagation delay} + \text{ProcessingDelay}

Example: satellite straight overhead at 600 km -> 600,000 / 299,792,458 = 2.0014 ms,
plus 1 ms processing = 3.0014 ms.

Association and update loop (pseudocode)
########################################

.. code-block:: text

   every UpdateInterval:                       # NtnLinkManager::Update()
     compute every satellite position once
     for each ground terminal g (registration order):
       candidates = []
       for each satellite s (registration order):
         elevation, range = geometry(g, s); delay = range / v + processing
         visible = elevation >= MinElevation
         if visible != previous visible(g, s): fire VisibilityChange(g, s, visible)
         if visible:
           fire SlantRangeUpdate(g, s, range); fire LinkDelayUpdate(g, s, delay)
           candidates.append(s, elevation, range, delay)
       decision = policy.Select({now, g, current association, candidates})
       abort if decision is not -1 and not in candidates      # guarantees SRS V3
       if decision != current association:
         update flags and counters; fire AssociationChange(g, old, new)
     fire UpdateCompleted(now)

   HighestElevationPolicy.Select(state):
     return candidate with the largest elevation (tie -> lowest node ID), or -1 if none

A terminal always has at most one association (SRS: one active association). With no visible
satellite the association becomes -1 (``ntn::NO_SATELLITE``); ``GetAssociatedSatellite()``
returns -1 and ``AssociationChange(g, old, -1)`` fires.

Network abstraction (IP links)
##############################

SRS section 3 allows an abstract IP link. ``NtnHelper::InstallIpLinks()`` creates one standard
ns-3 ``PointToPoint`` link per ground-satellite pair (data rate = link manager ``DataRate``),
assigns one /30 subnet per link, and gives every satellite an extra address on its loopback
interface, the **satellite service address** (default ``10.255.255.1``). The
``NtnIpLinkController`` then follows the link manager:

* on ``LinkDelayUpdate`` it sets the point-to-point channel ``Delay`` to the computed link delay
  (a packet keeps the delay that was valid when it was transmitted);
* on ``AssociationChange`` it sets the IPv4 interfaces of the old link down and the new link up
  (both ends) and moves the ground terminal's default route to the new satellite.

Traffic sent by a ground terminal to the service address therefore reaches whichever satellite
it is associated with, over a real IPv4/PPP path whose one-way delay is
``link delay + transmission time``. Test T6 checks this to 1 ns.

Performance
###########

Each update evaluates every ground-satellite pair: O(G x S). This is necessary because any
satellite may enter the view of any terminal. It runs once per ``UpdateInterval`` (default
100 ms). At the SRS demonstration size (20 x 12 = 240 pairs) the full 20-minute advanced example
with traffic runs in about 3 s. Possible future optimisations: skip satellites on the far side
of the Earth with one dot product before computing elevation; group satellites by orbit plane
and only test the arc near each terminal; adapt the update interval to the time-to-next-event.

Scope and Limitations
=====================

What is modelled: circular LEO orbits, fixed (or user-supplied moving) ground terminals,
geometric visibility, slant range, propagation + processing delay, one association per terminal
with a pluggable policy, periodic re-evaluation, handover, trace sources, IP traffic over
delay-following links, configurable logical data rate.

**Not modelled / limitations:**

* No 3GPP NR-NTN PHY/MAC, no OFDM numerology, no synchronisation or timing advance.
* No Doppler shift or Doppler compensation.
* No beamforming, antenna arrays, antenna gain or beam management.
* No atmospheric, ionospheric, rain or detailed channel/fading model; no link budget, so a
  visible satellite is always usable; packet loss on links is zero.
* No regenerative payload modelling (the satellite is a simple IP node).
* No standards conformance claim of any kind.
* Spherical Earth (R = 6,371 km), no oblateness; circular orbits only, no orbital perturbations
  (J2, drag).
* **Earth rotation is ignored** -- ground terminals are fixed in the Earth-centred frame.
* Visibility is purely geometric; terrain, buildings and other obstructions are ignored.
* Handover is instantaneous and break-before-make: a packet already in flight on the old link
  when the association changes is dropped at the satellite.
* Association and visibility are only re-evaluated every ``UpdateInterval``; events are quantised
  to that interval.
* Phase 1 has no gateway or inter-satellite links: traffic terminates at the associated
  satellite (Phase 2/3 extensions).
* The link ``DataRate`` is read when ``InstallIpLinks()`` is called; changing it later does not
  affect existing devices.
* ``NumSatellites`` is the size of the node container passed to
  ``NtnHelper::InstallSatellites`` (validated 1-200), not a separate Attribute; Phase 1 places
  all satellites in one orbit plane.

References
==========

* [R1] 3GPP, "Non-Terrestrial Networks (NTN)" overview, https://www.3gpp.org/technologies/ntn-overview
  (background only).
* [R2] 3GPP TR 38.811, "Study on New Radio (NR) to support non-terrestrial networks"
  (background only; not implemented).
* [R3] ns-3 Project, MobilityModel documentation, Release 3.46.
* Circular-orbit period and slant-range geometry: standard two-body / spherical-Earth relations
  found in any orbital-mechanics textbook.

Usage
*****

Helpers
=======

``NtnHelper`` performs each step explicitly (there is no "configure everything" call):

.. code-block:: cpp

   NtnHelper ntn;
   ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(600e3));
   ntn.SetVisibilityAttribute("MinElevation", DoubleValue(10.0));
   ntn.SetDelayAttribute("ProcessingDelay", TimeValue(MilliSeconds(1)));
   ntn.SetLinkAttribute("UpdateInterval", TimeValue(MilliSeconds(100)));
   ntn.SetLinkAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
   ntn.SetPolicy("ns3::HighestElevationPolicy");
   ntn.InstallSatellites(satellites);          // N satellites spread evenly on one ring
   ntn.InstallGroundTerminal(node, latDeg, lonDeg);
   InternetStackHelper().Install(allNodes);
   Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
   ntn.InstallIpLinks(addresses);
   ntn.EnableTracing("link-state.csv", "events.csv", Seconds(1));
   ntn.Start();

Attributes
==========

All ranges are project configuration ranges (not standards). Out-of-range values are rejected
by the Attribute checker (``SetAttribute`` stops with an error, ``SetAttributeFailSafe`` returns
false) and by an ``NS_ABORT_MSG_IF`` in each setter.

+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| SRS parameter            | Where                              | Default          | Range                | Unit                    |
+==========================+====================================+==================+======================+=========================+
| EarthRadius              | constant ``ntn::EARTH_RADIUS_M``   | 6,371,000        | fixed                | m                       |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| OrbitAltitude            | CircularOrbitMobilityModel         | 600,000          | 200,000 - 2,000,000  | m                       |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| OrbitInclination         | CircularOrbitMobilityModel         | 53               | 0 - 90               | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| InitialPhase             | CircularOrbitMobilityModel         | 0                | [0, 360)             | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (Raan, plane direction)  | CircularOrbitMobilityModel         | 0                | [0, 360)             | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| NumSatellites            | ``NtnHelper::InstallSatellites``   | 6 (SRS); set by  | 1 - 200              | count                   |
|                          | (node container size)              | example args     |                      |                         |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| NumOrbitalPlanes         | Phase 2 (R)                        | 1                | 1 - 20               | count                   |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| MinElevation             | NtnVisibilityModel                 | 10               | 0 - 45               | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| UpdateInterval           | NtnLinkManager                     | 100 ms           | 10 ms - 10 s         | Time                    |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| DataRate                 | NtnLinkManager                     | 100 Mbit/s       | 1 Mbit/s - 10 Gbit/s | DataRate                |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| ProcessingDelay          | NtnPropagationDelayModel           | 1 ms             | 0 - 100 ms           | Time                    |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| PropagationSpeed         | NtnPropagationDelayModel           | 299,792,458      | 1 - 299,792,458      | m/s                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| GroundHeight             | NtnGroundTerminalMobilityModel     | 0                | 0 - 5,000            | m                       |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (Latitude / Longitude)   | NtnGroundTerminalMobilityModel     | 0 / 0            | +-90 / +-180         | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| EnableGateway            | Phase 2 (R)                        | false            | true/false           | bool                    |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| MaxAssociationsPerGround | Phase 2 (R)                        | 1                | 1 - 4                | count                   |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (LinkStateSample-        | NtnTraceWriter                     | 0 (every update) | >= 0                 | Time                    |
| Interval)                |                                    |                  |                      |                         |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+

Traces
======

``NtnLinkManager`` trace sources:

* ``VisibilityChange(uint32_t groundId, uint32_t satelliteId, bool visible)`` -- a satellite
  crossed the ``MinElevation`` boundary (the first update reports every visible satellite).
* ``AssociationChange(uint32_t groundId, int32_t oldSatelliteId, int32_t newSatelliteId)`` --
  -1 means "none" (acquired: -1 -> s, handover: s1 -> s2, lost: s -> -1).
* ``SlantRangeUpdate(uint32_t groundId, uint32_t satelliteId, double rangeM)`` -- every update,
  for every visible satellite (sampling is done by the CSV writer, not the trace).
* ``LinkDelayUpdate(uint32_t groundId, uint32_t satelliteId, Time delay)`` -- every update, for
  every visible satellite; delay = propagation + processing.
* ``UpdateCompleted(Time now)`` -- end of every update.

Output files (``NtnHelper::EnableTracing``):

* Link-state CSV, one row per pair per sample:
  ``time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated``.
* Common event log (Common Guidelines 4.5), one row per event:
  ``time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail``. For
  ``VisibilityChange``: node = ground, peer = satellite, value1/value2 = elevation_deg/range_km.
  For ``AssociationChange``: node = ground, peer = new satellite, value1 = old satellite ID,
  value2 = delay_ms of the new link; detail = acquired / handover / lost. ``flow_id`` is empty
  (not flow related).

Queries for other modules: ``GetAssociatedSatellite()``, ``GetLinkState()`` (common
``LinkState`` contract), ``GetPairState()``, ``GetPairStates()``, ``GetVisibleSatellites()``,
``GetMobilityState()`` (common ``MobilityState`` contract), ``GetAssociationChangeCount()``,
``GetHandoverCount()``.

Extension points
================

* **New association policy** -- derive from ``NtnAssociationPolicy`` and implement
  ``NtnAssociationDecision Select(const NtnAssociationState& state)``; the state lists every
  visible candidate with elevation (deg), slant range (m) and link delay, plus the current
  association. Install with ``NtnHelper::SetPolicy(...)``. The link manager does not change.
* **Other network setups (e.g. M02)** -- use ``NtnLinkManager`` alone and subscribe to its
  traces (``AssociationChange``, ``LinkDelayUpdate``) instead of ``NtnIpLinkController``; or
  query ``GetLinkState()`` for the common ``LinkState`` view. M02 may depend on M01, not the
  reverse.
* **Moving ground terminals** -- give the node any ns-3 ``MobilityModel`` reporting
  Earth-centred metres and register it with ``NtnHelper::InstallGroundTerminals()``.
* ``model/ntn-common-types.h`` mirrors the FutureNet common ``LinkState`` / ``MobilityState``
  contract and should be replaced by ``futurenet-common`` types once supplied.

Examples
========

* ``ntn-example-basic`` -- one ground station + one satellite pass.
* ``ntn-example-advanced`` -- N ground terminals + ring of satellites with UDP traffic to the
  satellite service address, FlowMonitor statistics, both CSVs, handover counts. Runs the SRS
  demonstration size with ``--numGround=20 --numSats=12``.
* ``ntn-validation`` -- validation cases V1-V3 (below).

Common command-line options: ``--numGround``, ``--numSats``, ``--altitudeKm``,
``--inclinationDeg``, ``--minElevationDeg``, ``--updateInterval``, ``--dataRate``,
``--processingDelay``, ``--simulationTime``, ``--linkStateFile``, ``--eventLogFile``,
``--runInfoFile``, ``--csvSampleInterval``, plus ns-3's ``--RngSeed`` / ``--RngRun``.

Validation
**********

Methodology (Common Guidelines section 10): each case compares simulation output against a
closed-form result where one exists. Raw results are in ``doc/validation/`` together with
``validation-run-info.txt`` (ns-3 version, Git commit, seed, run, command line). Reproduce with
``./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"``.

* **V1 baseline (SRS V1)** -- satellite exactly overhead at 600 km:
  simulated link delay 3.001385 ms vs closed form 600 km / c + 1 ms = 3.0013846 ms; error
  0.43 ns (ns-3 ``Time`` has 1 ns resolution). PASS.
* **V2 one parameter varied** -- overhead propagation delay for 200 / 600 / 1,200 / 2,000 km:
  0.667 / 2.001 / 4.003 / 6.671 ms, equal to h/c and strictly increasing. PASS.
  **SRS V2** -- one pass, sampled every 10 s: range falls from 3,501 km to 600.3 km (closest
  sample) and rises again. PASS.
* **V3 boundary** -- ``MinElevation`` 0 and 45 deg at 200 and 2,000 km. Visibility duration
  compared with :math:`2\lambda/\omega`, :math:`\lambda = \arccos(R\cos e / a) - e`, and the
  largest visible range with :math:`\rho = \sqrt{a^2 - R^2\cos^2 e} - R\sin e`; all four cases
  agree within one update interval (e.g. 45 deg / 200 km: 50.6 s vs 50.607 s). PASS.
  **SRS V3** -- 20 terminals x 12 satellites for 20 min: 68 association changes, 0 to a
  non-visible satellite. PASS.

Testing
*******

``./test.py -s futurenet-ntn`` runs 11 test cases (all QUICK):

* T1 construction and SRS default values
* T2 / M01-T02 invalid attribute values rejected, boundaries accepted
* T3 / M01-T01 deterministic position vs hand-computed values, velocity consistency
* T3 orbit period at 600 km = 5,792.3 s (96.5 min)
* T3 / M01-T03 visibility flips at the elevation boundary (static and moving)
* T3 / M01-T04 / V1 delay = path / c (+ processing) within 1 ns
* T3 / M01-T05 highest-elevation policy picks the expected satellite; -1 with nothing visible
* T4 / M01-FR-08 every trace source fires with correct arguments
* T5 / M01-T06 two runs give byte-identical CSV output
* T6 / M01-FR-09 packets over IPv4 + PointToPoint arrive after exactly link delay + transmission
  time at the configured data rate
* V2 / V3 / AC-05 pass shape, association only to visible satellites, at least one handover
