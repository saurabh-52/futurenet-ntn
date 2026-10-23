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
links whose delay follows the geometry. Phase 2 adds multi-plane (Walker) constellations, up to
four simultaneous associations per terminal, a second association policy, and gateway ground
stations with a real ground -> satellite -> gateway IP path. Phase 3 adds inter-satellite links
(ISLs) with a range and Earth-blocking rule.

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
                                 | TraceSources             |  <- StickyAssociation-    |
                                 |                          |     Policy                |
                                 |                          +---------------------------+
               +-----------------+------------------+
               v                                    v
     +---------------------+              +------------------+
     | NtnIpLinkController |              | NtnTraceWriter   |
     | (IPv4 up/down,      |              | (link-state CSV, |
     |  p2p channel Delay, |              |  common event    |
     |  default route)     |              |  log)            |
     +---------------------+              +------------------+

     Gateway nodes (NtnGroundTerminalMobilityModel) are registered with the NtnLinkManager like
     ground terminals; NtnWalkerConstellation computes each satellite's plane and orbit angles.
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
     NtnAssociationPolicy <|-- StickyAssociationPolicy
     Object <|-- NtnWalkerConstellation
     Object <|-- NtnIslModel
     NtnLinkManager o-- NtnIslModel
     Object <|-- NtnLinkManager
     Object <|-- NtnIpLinkController
     Object <|-- NtnTraceWriter
     NtnLinkManager o-- NtnVisibilityModel
     NtnLinkManager o-- NtnPropagationDelayModel
     NtnLinkManager o-- NtnAssociationPolicy
     NtnIpLinkController ..> NtnLinkManager : listens to traces
     NtnTraceWriter ..> NtnLinkManager : listens to traces
     NtnHelper ..> NtnLinkManager : creates
     NtnHelper ..> NtnWalkerConstellation : uses for InstallSatellites

Responsibilities (one per class, CG-05):

* ``CircularOrbitMobilityModel`` -- satellite position/velocity as a pure function of time.
* ``NtnGroundTerminalMobilityModel`` -- fixed terminal placed by latitude/longitude/height.
* ``NtnVisibilityModel`` -- elevation, slant range and the ``MinElevation`` visibility rule.
* ``NtnPropagationDelayModel`` -- path length to delay (``PropagationSpeed``,
  ``ProcessingDelay``).
* ``NtnWalkerConstellation`` -- divides the satellites between orbital planes (Phase 2).
* ``NtnIslModel`` -- decides whether two satellites can have an inter-satellite link (Phase 3).
* ``NtnAssociationPolicy`` / ``HighestElevationPolicy`` / ``StickyAssociationPolicy`` -- choose the
  satellite(s) of a terminal.
* ``NtnLinkManager`` -- periodic update loop, per-pair state (ground and gateway pairs), trace
  sources, state queries.
* ``NtnIpLinkController`` -- applies associations and delays to real IPv4 point-to-point links
  (ground and gateway links, routes).
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

Multi-plane (Walker) constellations
###################################

``NtnWalkerConstellation`` (SRS M01-FR-10) lays out N satellites in P = ``NumOrbitalPlanes``
planes. All planes share altitude and inclination and differ only in RAAN. With S = N / P
satellites per plane, satellite k is in plane p = k / S at slot j = k mod S and gets

.. math::

   \Omega_p = \Omega_0 + p\,\frac{\text{RaanSpread}}{P}, \qquad
   u_{0,p,j} = u_0 + j\,\frac{360}{S} + p \cdot \text{PlanePhaseOffset}

(both wrapped into [0, 360)). In words: the planes are fanned out evenly, the satellites of a plane
are evenly spaced around it, and each plane is shifted forward by ``PlanePhaseOffset`` so that
neighbouring planes do not pass over the same spot at once. ``RaanSpread`` = 360 deg gives a
*Walker delta* (typical for inclined orbits such as 53 deg), 180 deg a *Walker star* (near-polar
orbits). In Walker notation i:N/P/F, ``PlanePhaseOffset`` = F x 360 / N.

Example: 6 satellites, 3 planes, offset 20 deg, Raan0 = 10, phase0 = 5 -> planes at RAAN 10, 130,
250 deg; phases 5/185, 25/205, 45/225 deg. N must be a multiple of P (otherwise the helper stops
with a clear message). With P = 1 the layout is exactly the Phase 1 single ring (bit-identical).

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
     if gateways are registered: UpdateGateways()   # see "Gateways" below
     if EnableIsl: UpdateIsls()                      # see "Inter-satellite links" below
     for each ground terminal g (registration order):
       candidates = []
       for each satellite s (registration order):
         elevation, range = geometry(g, s); delay = range / v + processing
         visible = elevation >= MinElevation
         if visible != previous visible(g, s): fire VisibilityChange(g, s, visible)
         if visible:
           fire SlantRangeUpdate(g, s, range); fire LinkDelayUpdate(g, s, delay)
           candidates.append(s, elevation, range, delay)
       decision = policy.Select({now, g, current primary, current secondaries,
                                 MaxAssociationsPerGround, candidates})
       abort unless: primary is -1 or a candidate; secondaries are distinct candidates, not the
                     primary, at most Max - 1 of them, and empty when primary is -1   # SRS V3
       store the new association set (pair "associated" flags) first, then fire:
         SecondaryAssociationChange(g, s, false) for each removed secondary
         AssociationChange(g, old, new)            if the primary changed (counters updated)
         SecondaryAssociationChange(g, s, true)  for each added secondary
     fire UpdateCompleted(now)

   HighestElevationPolicy.Select(state):
     rank candidates by elevation (tie -> lowest node ID)
     primary = first, secondaries = next Max - 1; -1 if no candidate

   StickyAssociationPolicy.Select(state):
     primary = current primary if still visible
               else the highest still-visible current secondary   # make-before-break
               else the highest candidate
     secondaries = still-visible current secondaries, then the highest remaining candidates

Each terminal has one **primary** association (SRS: one active association; it carries the
default route and is what ``GetAssociatedSatellite()`` and ``AssociationChange`` report) and, with
``MaxAssociationsPerGround`` = K > 1, up to K - 1 **secondary** associations whose links are kept
up but carry no default traffic. With the default K = 1 the behaviour is exactly Phase 1. With no
visible satellite the primary becomes -1 (``ntn::NO_SATELLITE``); ``GetAssociatedSatellite()``
returns -1 and ``AssociationChange(g, old, -1)`` fires.

*Why a second policy?* The baseline switches as soon as another satellite climbs higher, so a
terminal can hand over more often than necessary. The sticky policy stays until the satellite is
lost. It makes fewer handovers but uses lower (farther) satellites on average -- see V4.

Gateways
########

With ``EnableGateway`` = true and gateways registered (``NtnHelper::InstallGateway``), every
update first runs the same geometry for every gateway-satellite pair (SRS M01-FR-11):

.. code-block:: text

   UpdateGateways():
     for each satellite s:
       for each gateway w: elevation, range, delay = geometry(w, s); visible = elev >= MinElevation
         fire GatewayVisibilityChange(w, s, visible) on a change
         if visible: fire GatewayLinkDelayUpdate(w, s, delay)
       best = visible gateway with the highest elevation (tie -> lowest node ID), or -1
       if best != current gateway of s: fire GatewayAssociationChange(s, old, best)

The gateway rule is fixed (highest elevation) rather than a pluggable policy; satellites are not
limited in how many terminals they serve. ``GetEndToEndPath(g)`` returns the path ground -> primary
satellite -> its gateway with both hop delays and their sum (``available`` = false if a hop is
missing). Gateways run first in each update, so listeners of ``AssociationChange`` already see the
current gateway hop.

Inter-satellite links
#####################

With ``EnableIsl`` = true, every update checks every satellite pair (SRS M01-FR-12) with
``NtnIslModel``. A pair has an ISL when

1. the distance is at most ``MaxIslRange`` (default 5,000 km; stands in for terminal power and
   pointing limits), and
2. the straight line between the satellites stays at least ``IslMinClearance`` (default 0 m)
   above the Earth surface -- with 0 this is exactly "the line does not pass through the Earth";
   a margin can model the atmosphere.

*Clearance geometry.* For the segment A -> B the point closest to the Earth centre is
:math:`P = A + t(B - A)` with :math:`t = -A\cdot(B-A)/|B-A|^2` **clamped to [0, 1]** (the closest
point of the infinite line may lie outside the segment, e.g. when both satellites are on one
radius); clearance = :math:`|P| - R`. For two satellites on one ring of radius *a*, an angle
:math:`\theta` apart, the lowest point is the middle of the chord: distance
:math:`2a\sin(\theta/2)`, clearance :math:`a\cos(\theta/2) - R`. At 600 km: 30 deg -> 3,608.5 km,
362.5 km clear (ISL up); 60 deg -> the line passes 333.9 km *below* the surface (blocked). The
longest ISL any two 600 km satellites can have is the line-of-sight limit
:math:`2\sqrt{a^2 - R^2}` = 5,658.7 km.

.. code-block:: text

   UpdateIsls():
     for each satellite pair (A, B), lower node ID first:
       distance = |A - B|; clearance = segment clearance; delay = distance / v + processing
       up = distance <= MaxIslRange and clearance >= IslMinClearance
       if up != previous: fire IslStateChange(A, B, up)
       if up: fire IslDelayUpdate(A, B, delay)

The ISL delay uses the same ``NtnPropagationDelayModel`` as the other hops. The module reports
which ISLs exist and their delay (``GetIslState``, ``GetIslNeighbours``, ``GetIslLinkState``);
**routing over ISLs is not done here** -- that is the job of a routing module such as M02, which
can build its graph from these queries and traces. ``ntn-example-isl`` shows such an analysis
(shortest delay to a gateway) without sending packets.

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

On ``SecondaryAssociationChange`` the controller brings the secondary link up or down (a
satellite that only moves between primary and secondary stays up).

With gateways, ``InstallIpLinks()`` also creates one link per gateway-satellite pair and gives
every gateway the **gateway service address** (default ``10.255.254.1``) on its loopback. On
``GatewayAssociationChange`` the controller brings the old gateway link down and the new one up,
points the satellite's default route at the new gateway, and adds routes on the new gateway back
to every ground-link subnet of that satellite. (ns-3's ``Ipv4StaticRouting`` deletes every route
through an interface that goes down, so the old gateway's routes disappear by themselves.) A
packet to the gateway service address thus travels terminal -> primary satellite -> gateway, and
a reply comes back the same way; the gateway-path T6 test checks both directions to 1 ns.

Performance
###########

Each update evaluates every ground-satellite pair: O(G x S), plus O(W x S) for W gateways. This
is necessary because any satellite may enter the view of any terminal or gateway. It runs once per
``UpdateInterval`` (default 100 ms). With ISLs, every satellite pair is checked too:
O(S^2 / 2) = 7,140 pairs for 120 satellites, which dominates; the ISL example therefore uses
1 s updates (20 min in about 5 s instead of 26 s with 100 ms). At the SRS demonstration size (20 x 12 = 240 pairs) the full
20-minute advanced example with traffic runs in about 3 s; the constellation example (20
terminals + 3 gateways x 120 satellites = 2,760 pairs, 1,380 IP links) takes about 12 s. Possible future optimisations: skip satellites on the far side
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
* ISLs are modelled as link state (existence, distance, delay, traces) only; no packets are
  routed over them, so in the IP model a satellite still only forwards between its ground links
  and its gateway link, and traffic is dropped if the serving satellite sees no gateway.
* The ISL rule is purely geometric (range + Earth clearance): no limit on the number of ISL
  terminals per satellite, no pointing/acquisition time, no ISL capacity or link budget.
* Optional extras not built (by decision): Earth rotation, link budget, energy model.
* Gateways are not connected to each other (no terrestrial backbone); replies return only through
  the satellite whose gateway received the request.
* The gateway choice is a fixed rule (highest elevation); satellites and gateways have no
  capacity limit.
* Secondary associations keep their links up but the default route always uses the primary
  satellite; there is no multipath scheduling.
* The link ``DataRate`` is read when ``InstallIpLinks()`` is called; changing it later does not
  affect existing devices.
* ``NumSatellites`` is the size of the node container passed to
  ``NtnHelper::InstallSatellites`` (validated 1-200), not a separate Attribute; it must be a
  multiple of ``NumOrbitalPlanes``. All planes of one ``InstallSatellites`` call share altitude
  and inclination (call it again for another shell).

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
   ntn.SetPolicy("ns3::HighestElevationPolicy");          // or "ns3::StickyAssociationPolicy"
   ntn.SetConstellationAttribute("NumOrbitalPlanes", UintegerValue(10));   // Phase 2
   ntn.SetConstellationAttribute("PlanePhaseOffset", DoubleValue(3.0));
   ntn.SetLinkAttribute("MaxAssociationsPerGround", UintegerValue(1));
   ntn.SetLinkAttribute("EnableGateway", BooleanValue(true));
   ntn.InstallSatellites(satellites);          // N satellites over the planes
   ntn.InstallGroundTerminal(node, latDeg, lonDeg);
   ntn.InstallGateway(gatewayNode, latDeg, lonDeg);
   ntn.SetLinkAttribute("EnableIsl", BooleanValue(true));                 // Phase 3
   ntn.SetIslAttribute("MaxIslRange", DoubleValue(5000e3));
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
| NumOrbitalPlanes         | NtnWalkerConstellation             | 1                | 1 - 20               | count                   |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (PlanePhaseOffset)       | NtnWalkerConstellation             | 0                | [0, 360)             | deg                     |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (RaanSpread)             | NtnWalkerConstellation             | 360              | (0, 360]             | deg                     |
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
| EnableGateway            | NtnLinkManager                     | false            | true/false           | bool                    |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| MaxAssociationsPerGround | NtnLinkManager                     | 1                | 1 - 4                | count                   |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| EnableIsl (Phase 3)      | NtnLinkManager                     | false            | true/false           | bool                    |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (MaxIslRange)            | NtnIslModel                        | 5,000,000        | 100,000 - 20,000,000 | m                       |
+--------------------------+------------------------------------+------------------+----------------------+-------------------------+
| (IslMinClearance)        | NtnIslModel                        | 0                | 0 - 1,000,000        | m                       |
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
* ``SecondaryAssociationChange(uint32_t groundId, uint32_t satelliteId, bool associated)`` --
  a satellite was added to / removed from the secondary associations (only with
  ``MaxAssociationsPerGround`` > 1).
* ``GatewayVisibilityChange(uint32_t gatewayId, uint32_t satelliteId, bool visible)``.
* ``GatewayAssociationChange(uint32_t satelliteId, int32_t oldGatewayId, int32_t newGatewayId)``
  -- -1 means "none".
* ``GatewayLinkDelayUpdate(uint32_t gatewayId, uint32_t satelliteId, Time delay)`` -- every
  update, for every visible gateway-satellite pair.
* ``IslStateChange(uint32_t satelliteA, uint32_t satelliteB, bool up)`` -- an ISL became possible
  or was lost (satelliteA < satelliteB).
* ``IslDelayUpdate(uint32_t satelliteA, uint32_t satelliteB, Time delay)`` -- every update, for
  every ISL that is up.

Output files (``NtnHelper::EnableTracing``):

* Link-state CSV, one row per pair per sample:
  ``time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated``.
* Common event log (Common Guidelines 4.5), one row per event:
  ``time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail``. For
  ``VisibilityChange``: node = ground, peer = satellite, value1/value2 = elevation_deg/range_km.
  For ``AssociationChange``: node = ground, peer = new satellite, value1 = old satellite ID,
  value2 = delay_ms of the new link; detail = acquired / handover / lost. ``flow_id`` is empty
  (not flow related).
  ``SecondaryAssociationChange``: node = ground, peer = satellite, value1/value2 =
  elevation_deg/delay_ms, detail = added / removed. ``GatewayVisibilityChange``: node = gateway,
  peer = satellite, as ``VisibilityChange``. ``GatewayAssociationChange``: node = satellite,
  peer = new gateway, value1 = old gateway ID, value2 = delay_ms, detail as
  ``AssociationChange``.
* With gateways the link-state CSV also has one row per gateway-satellite pair after the ground
  rows; ``ground_id`` then holds the gateway node ID and ``associated`` = 1 means the satellite
  uses that gateway. Without gateways the files are exactly as in Phase 1.
* ``IslStateChange`` event-log rows: node = satelliteA, peer = satelliteB, value1/value2 =
  distance_km/clearance_km, detail = up / down.
* ISL CSV (4th argument of ``EnableTracing``, same sample interval), one row per ISL that is up:
  ``time_s,satellite_a,satellite_b,distance_km,clearance_km,delay_ms``.

Queries: see "API for other modules" below.

API for other modules
=====================

This section is the interface the Family A teams (M02, M03) can review. Another module only needs
the public methods, trace sources and state structs below -- never the internals -- and may
depend on ``futurenet-ntn`` (M01 never depends on M02/M03).

*Setting up.* Either use ``NtnHelper`` (it creates the ``NtnLinkManager``; get it with
``GetLinkManager()``), or create an ``NtnLinkManager`` yourself and register nodes with
``AddGroundNode``, ``AddSatelliteNode`` and ``AddGatewayNode``. Call ``Start()`` once. Skip
``InstallIpLinks()`` if your module builds its own network; the manager then only computes and
reports state.

*State queries* (``NtnLinkManager``, all ``const``, valid after the first update):

+--------------------------------------------------+------------------------------------------------+
| Method                                           | Returns                                        |
+==================================================+================================================+
| ``GetAssociatedSatellite(groundId)``             | primary satellite node ID, -1 = none           |
+--------------------------------------------------+------------------------------------------------+
| ``GetAssociatedSatellites(groundId)``            | primary first, then secondary IDs              |
+--------------------------------------------------+------------------------------------------------+
| ``GetVisibleSatellites(groundId)``               | ``NtnCandidate`` list: ID, elevation, range,   |
|                                                  | delay (SRS M01-FR-04)                          |
+--------------------------------------------------+------------------------------------------------+
| ``GetLinkState(groundId, satelliteId)``          | ``NtnLinkState`` = common ``LinkState`` view   |
+--------------------------------------------------+------------------------------------------------+
| ``GetPairState`` / ``GetPairStates``             | ``NtnPairState``: full geometry + flags        |
+--------------------------------------------------+------------------------------------------------+
| ``GetMobilityState(nodeId)``                     | ``NtnMobilityState`` = common ``MobilityState``|
+--------------------------------------------------+------------------------------------------------+
| ``GetSatelliteGateway(satelliteId)``             | gateway node ID, -1 = none                     |
+--------------------------------------------------+------------------------------------------------+
| ``GetGatewayPairState`` / ``...States``          | gateway-satellite geometry (groundId = gateway)|
+--------------------------------------------------+------------------------------------------------+
| ``GetEndToEndPath(groundId)``                    | ``NtnEndToEndPath``: satellite, gateway, both  |
|                                                  | hop delays, total, available                   |
+--------------------------------------------------+------------------------------------------------+
| ``GetIslState(a, b)`` / ``GetIslStates()``       | ``NtnIslState``: distance, clearance,          |
|                                                  | delay, up (Phase 3)                            |
+--------------------------------------------------+------------------------------------------------+
| ``GetIslNeighbours(satelliteId)``                | satellites with an ISL that is up              |
+--------------------------------------------------+------------------------------------------------+
| ``GetIslLinkState(a, b)``                        | ISL as common ``LinkState``                    |
+--------------------------------------------------+------------------------------------------------+
| ``GetAssociationChangeCount`` / ``GetHandover-`` | counters per terminal / per satellite          |
| ``Count`` / ``GetGatewayChangeCount``            |                                                |
+--------------------------------------------------+------------------------------------------------+

*Events* -- connect with ``TraceConnectWithoutContext(name, MakeCallback(...))``; the signatures
are the ``...TracedCallback`` typedefs in ``ntn-link-manager.h``: ``VisibilityChange``,
``AssociationChange``, ``SecondaryAssociationChange``, ``SlantRangeUpdate``, ``LinkDelayUpdate``,
``GatewayVisibilityChange``, ``GatewayAssociationChange``, ``GatewayLinkDelayUpdate``,
``IslStateChange``, ``IslDelayUpdate``, ``UpdateCompleted``. Ordering guarantee: within one
update, gateway events come first, then ISL events, then ground events, and when an association trace fires the stored state (``GetPairState``, ``associated``
flags, ``GetEndToEndPath``) already reflects the new decision.

*Decisions* -- to plug in an optimisation/RL policy, derive from ``NtnAssociationPolicy`` and
implement ``Select(const NtnAssociationState&)``; the manager validates every decision (only
visible satellites, at most ``MaxAssociationsPerGround``) and aborts with a clear message
otherwise.

*Common types* -- ``NtnLinkState`` and ``NtnMobilityState`` mirror the FutureNet common
``LinkState`` / ``MobilityState`` contract field for field (units in the field names / comments)
and will be swapped for ``futurenet-common`` types when that package is supplied.

*Example for M02 (routing):* listen to ``AssociationChange`` and ``GatewayAssociationChange`` to
update routes, and read ``GetEndToEndPath(g).totalDelay`` as the current access delay of terminal
g. With ISLs, build the satellite graph from ``GetIslStates()`` (edges that are ``up``, weight
``linkDelay``) or keep it current with ``IslStateChange`` / ``IslDelayUpdate``.

Extension points
================

* **New association policy** -- derive from ``NtnAssociationPolicy`` and implement
  ``NtnAssociationDecision Select(const NtnAssociationState& state)``; the state lists every
  visible candidate with elevation (deg), slant range (m) and link delay, plus the current primary
  and secondary associations and ``maxAssociations``. ``StickyAssociationPolicy`` is a worked
  example. Install with ``NtnHelper::SetPolicy(...)``. The link manager does not change.
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
* ``ntn-example-constellation`` -- 120 satellites in 10 Walker planes, 3 gateways, 20 terminals
  sending UDP echo requests to the gateway service address (terminal -> satellite -> gateway and
  back), FlowMonitor for both directions; ``--policy=highest|sticky``, ``--maxAssociations``,
  ``--numPlanes``, ``--walkerF``, ``--numGateways``, region options.
* ``ntn-example-isl`` -- the same Walker constellation with ISLs: per-minute table of ISLs up,
  neighbours per satellite, terminals with a direct gateway path and with a gateway reachable
  through ISLs (shortest-delay search inside the example); ``--maxIslRangeKm``,
  ``--islMinClearanceKm``; writes the ISL CSV and event log.
* ``ntn-validation`` -- validation cases V1-V5 (below).

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
* **V4 policy comparison (Phase 2)** -- Walker 53 deg: 120 satellites / 10 planes / F = 1, 20
  terminals between 45 deg S and 45 deg N, 1 h, update 1 s; same scenario with each policy:

  ===========================  =========  ==============  ===============  ==========
  Policy                       Handovers  Coverage        Mean elevation   Mean delay
  ===========================  =========  ==============  ===============  ==========
  HighestElevationPolicy       336        99.99 %         30.93 deg        4.927 ms
  StickyAssociationPolicy      299        99.99 %         24.91 deg        5.479 ms
  ===========================  =========  ==============  ===============  ==========

  Expected and observed: the sticky policy makes fewer handovers (-11 %), the baseline has the
  lower mean delay (at one altitude the highest satellite is always the nearest), coverage is
  identical. PASS. In a sparse constellation (24 satellites, 47 % coverage) both policies made the
  same 24 handovers -- terminals rarely see two satellites, so there is nothing to choose.
* **V5 inter-satellite links (Phase 3)** -- (a) 12 satellites on one 600 km ring: neighbour
  distance 3,608.455 km = :math:`2a\sin 15^\circ`, link delay 13.0365 ms = distance / c + 1 ms,
  largest error 0.31 ns. (b) Walker 120/10/1 with unlimited range for 1 h: in all 404,850 "up"
  samples and every "down" sample the ISL state equals "the line clears the Earth" (0
  inconsistent), and the longest ISL, 5,658.68 km, lies just below the line-of-sight limit
  :math:`2\sqrt{a^2-R^2}` = 5,658.69 km. PASS.

Testing
*******

``./test.py -s futurenet-ntn`` runs 17 test cases (all QUICK); the first 11 are the unchanged
Phase 1 cases, then 5 Phase 2 cases and 1 Phase 3 case:

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
* T1 T2 T3 M01-FR-10 Walker layout: defaults, rejected values, hand-computed angles and positions
* T1-T4 ``MaxAssociationsPerGround``: default/range, ranking, traces, and IP link state equal to
  the association set at every update of a 20-minute run
* T3 ``StickyAssociationPolicy`` rules and a scripted two-satellite pass vs the baseline
* T1 T3 T4 M01-FR-11 gateway choice, end-to-end delay, traces and output rows
* T6 M01-FR-11 UDP packets ground -> satellite -> gateway and back with exact arrival times
* T1-T4 M01-FR-12 ISL defaults/range checks, clearance vs hand-computed geometry (incl. the
  segment end-point case), manager state and neighbours, drop times by range (5,519 s) and by
  Earth blocking (7,825 s), event-log and ISL CSV rows
