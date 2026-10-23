/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn-tests
 * Test suite "futurenet-ntn".
 *
 * Each test case name starts with the Common Guidelines category (T1-T6) and the SRS IDs it
 * covers, e.g. "T3 M01-T01 ...". Expected values were computed independently by hand / with a
 * calculator from the formulas in doc/futurenet-ntn.rst (see the comments next to them).
 */

#include "ns3/circular-orbit-mobility-model.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/geographic-positions.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/node-container.h"
#include "ns3/ntn-association-policy.h"
#include "ns3/ntn-constants.h"
#include "ns3/ntn-ground-terminal-mobility-model.h"
#include "ns3/ntn-helper.h"
#include "ns3/ntn-ip-link-controller.h"
#include "ns3/ntn-link-manager.h"
#include "ns3/ntn-propagation-delay-model.h"
#include "ns3/ntn-trace-writer.h"
#include "ns3/ntn-visibility-model.h"
#include "ns3/ntn-walker-constellation.h"
#include "ns3/packet.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/test.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>

using namespace ns3;

/**
 * @defgroup futurenet-ntn-tests Tests for futurenet-ntn
 * @ingroup futurenet-ntn
 * @ingroup tests
 */

namespace
{

/// Orbit radius for the default 600 km altitude: 6,371,000 + 600,000 [m].
constexpr double ORBIT_RADIUS_600_KM_M = 6971000.0;

/// Orbit period at 600 km: 2*pi*sqrt(6971000^3 / 3.986004418e14) = 5792.334 s.
constexpr double PERIOD_600_KM_S = 5792.334;

/// Satellite speed at 600 km: 2*pi*6971000 / 5792.334 = 7561.733 m/s.
constexpr double SPEED_600_KM_MPS = 7561.733;

/// Earth-centre angle between a terminal and a 600 km satellite seen at 10 deg elevation:
/// acos(6371/6971 * cos 10deg) - 10deg = 15.836083 deg.
constexpr double CENTRAL_ANGLE_600_KM_10_DEG = 15.836083104335541;

/// One nanosecond in seconds: tolerance for delay comparisons (M01-T04).
constexpr double ONE_NS_S = 1e-9;

/// Position tolerance [m] (well above double rounding at ~7e6 m, well below any real error).
constexpr double POSITION_TOLERANCE_M = 1e-3;

/**
 * Create a node with a fixed ground terminal at the given coordinates.
 * @param latitudeDeg latitude [deg]
 * @param longitudeDeg longitude [deg]
 * @return the node
 */
Ptr<Node>
CreateGroundNode(double latitudeDeg, double longitudeDeg)
{
    Ptr<Node> node = CreateObject<Node>();
    Ptr<NtnGroundTerminalMobilityModel> mobility = CreateObject<NtnGroundTerminalMobilityModel>();
    mobility->SetLatitude(latitudeDeg);
    mobility->SetLongitude(longitudeDeg);
    node->AggregateObject(mobility);
    return node;
}

/**
 * Create a node with a 600 km circular orbit.
 * @param inclinationDeg inclination [deg]
 * @param initialPhaseDeg initial phase [deg]
 * @return the node
 */
Ptr<Node>
CreateSatelliteNode(double inclinationDeg, double initialPhaseDeg)
{
    Ptr<Node> node = CreateObject<Node>();
    Ptr<CircularOrbitMobilityModel> orbit = CreateObject<CircularOrbitMobilityModel>();
    orbit->SetInclination(inclinationDeg);
    orbit->SetInitialPhase(initialPhaseDeg);
    node->AggregateObject(orbit);
    return node;
}

/**
 * Read a whole file into a string.
 * @param path file path
 * @return the file content
 */
std::string
ReadFile(const std::string& path)
{
    std::ifstream in(path);
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

} // namespace

/**
 * @ingroup futurenet-ntn-tests
 * T1: every class constructs and every Attribute default equals the SRS section 7 value.
 */
class NtnDefaultsTestCase : public TestCase
{
  public:
    NtnDefaultsTestCase()
        : TestCase("T1 construction and SRS default values")
    {
    }

  private:
    void DoRun() override
    {
        DoubleValue d;
        TimeValue t;
        DataRateValue r;

        Ptr<CircularOrbitMobilityModel> orbit = CreateObject<CircularOrbitMobilityModel>();
        orbit->GetAttribute("OrbitAltitude", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 600e3, "OrbitAltitude default must be 600 km");
        orbit->GetAttribute("OrbitInclination", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 53.0, "OrbitInclination default must be 53 deg");
        orbit->GetAttribute("InitialPhase", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 0.0, "InitialPhase default must be 0 deg");
        orbit->GetAttribute("Raan", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 0.0, "Raan default must be 0 deg");

        Ptr<NtnGroundTerminalMobilityModel> ground = CreateObject<NtnGroundTerminalMobilityModel>();
        ground->GetAttribute("GroundHeight", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 0.0, "GroundHeight default must be 0 m");

        Ptr<NtnVisibilityModel> visibility = CreateObject<NtnVisibilityModel>();
        visibility->GetAttribute("MinElevation", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 10.0, "MinElevation default must be 10 deg");

        Ptr<NtnPropagationDelayModel> delay = CreateObject<NtnPropagationDelayModel>();
        delay->GetAttribute("PropagationSpeed", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 299792458.0, "PropagationSpeed default must be c");
        delay->GetAttribute("ProcessingDelay", t);
        NS_TEST_ASSERT_MSG_EQ(t.Get(), MilliSeconds(1), "ProcessingDelay default must be 1 ms");

        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        manager->GetAttribute("UpdateInterval", t);
        NS_TEST_ASSERT_MSG_EQ(t.Get(), MilliSeconds(100), "UpdateInterval default must be 100 ms");
        manager->GetAttribute("DataRate", r);
        NS_TEST_ASSERT_MSG_EQ(r.Get(), DataRate("100Mbps"), "DataRate default must be 100 Mbit/s");
        NS_TEST_ASSERT_MSG_NE(DynamicCast<HighestElevationPolicy>(manager->GetPolicy()),
                              nullptr,
                              "Default policy must be HighestElevationPolicy");

        // EarthRadius is a fixed constant; it must equal the sphere used for ground positions.
        NS_TEST_ASSERT_MSG_EQ(ntn::EARTH_RADIUS_M,
                              6371000.0,
                              "EarthRadius must be the SRS value 6,371,000 m");
        NS_TEST_ASSERT_MSG_EQ(ntn::EARTH_RADIUS_M,
                              GeographicPositions::EARTH_SPHERE_RADIUS,
                              "EarthRadius must match ns-3's spherical Earth");

        NS_TEST_ASSERT_MSG_NE(CreateObject<NtnIpLinkController>(), nullptr, "controller");
        NS_TEST_ASSERT_MSG_NE(CreateObject<NtnTraceWriter>(), nullptr, "trace writer");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T2 / M01-T02: out-of-range values are rejected, boundary values are accepted.
 */
class NtnAttributeValidationTestCase : public TestCase
{
  public:
    NtnAttributeValidationTestCase()
        : TestCase("T2 M01-T02 invalid attribute values are rejected")
    {
    }

  private:
    /**
     * Check that an Attribute accepts or rejects a value.
     * @param object object to configure
     * @param name attribute name
     * @param value value to try
     * @param accepted expected outcome
     */
    void Check(Ptr<Object> object,
               const std::string& name,
               const AttributeValue& value,
               bool accepted)
    {
        NS_TEST_EXPECT_MSG_EQ(object->SetAttributeFailSafe(name, value),
                              accepted,
                              name << " = " << value.SerializeToString(nullptr)
                                   << (accepted ? " must be accepted" : " must be rejected"));
    }

    void DoRun() override
    {
        Ptr<CircularOrbitMobilityModel> orbit = CreateObject<CircularOrbitMobilityModel>();
        Check(orbit, "OrbitAltitude", DoubleValue(199999.0), false);
        Check(orbit, "OrbitAltitude", DoubleValue(2000001.0), false);
        Check(orbit, "OrbitAltitude", DoubleValue(-600e3), false);
        Check(orbit, "OrbitAltitude", DoubleValue(200e3), true);
        Check(orbit, "OrbitAltitude", DoubleValue(2000e3), true);
        Check(orbit, "OrbitInclination", DoubleValue(-0.1), false);
        Check(orbit, "OrbitInclination", DoubleValue(90.1), false);
        Check(orbit, "OrbitInclination", DoubleValue(90.0), true);
        Check(orbit, "InitialPhase", DoubleValue(-1.0), false);
        Check(orbit, "InitialPhase", DoubleValue(360.0), false);
        Check(orbit, "InitialPhase", DoubleValue(359.9), true);
        Check(orbit, "Raan", DoubleValue(360.0), false);

        Ptr<NtnVisibilityModel> visibility = CreateObject<NtnVisibilityModel>();
        Check(visibility, "MinElevation", DoubleValue(-0.1), false);
        Check(visibility, "MinElevation", DoubleValue(45.1), false);
        Check(visibility, "MinElevation", DoubleValue(0.0), true);
        Check(visibility, "MinElevation", DoubleValue(45.0), true);

        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        Check(manager, "UpdateInterval", TimeValue(MilliSeconds(9)), false);
        Check(manager, "UpdateInterval", TimeValue(Seconds(10.001)), false);
        Check(manager, "UpdateInterval", TimeValue(MilliSeconds(10)), true);
        Check(manager, "UpdateInterval", TimeValue(Seconds(10)), true);
        Check(manager, "DataRate", DataRateValue(DataRate("999kbps")), false);
        Check(manager, "DataRate", DataRateValue(DataRate("10001Mbps")), false);
        Check(manager, "DataRate", DataRateValue(DataRate("1Mbps")), true);
        Check(manager, "DataRate", DataRateValue(DataRate("10Gbps")), true);

        Ptr<NtnPropagationDelayModel> delay = CreateObject<NtnPropagationDelayModel>();
        Check(delay, "ProcessingDelay", TimeValue(MilliSeconds(101)), false);
        Check(delay, "ProcessingDelay", TimeValue(MilliSeconds(-1)), false);
        Check(delay, "ProcessingDelay", TimeValue(Time(0)), true);
        Check(delay, "PropagationSpeed", DoubleValue(0.0), false);
        Check(delay, "PropagationSpeed", DoubleValue(3e8), false);
        Check(delay, "PropagationSpeed", DoubleValue(2e8), true);

        Ptr<NtnGroundTerminalMobilityModel> ground = CreateObject<NtnGroundTerminalMobilityModel>();
        Check(ground, "GroundHeight", DoubleValue(-1.0), false);
        Check(ground, "GroundHeight", DoubleValue(5001.0), false);
        Check(ground, "GroundHeight", DoubleValue(5000.0), true);
        Check(ground, "Latitude", DoubleValue(90.1), false);
        Check(ground, "Longitude", DoubleValue(-180.1), false);

        // A rejected value must leave the previous (valid) value in place.
        DoubleValue altitude;
        orbit->GetAttribute("OrbitAltitude", altitude);
        NS_TEST_ASSERT_MSG_EQ(altitude.Get(), 2000e3, "rejected values must not be stored");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T3 / M01-T01: the satellite position at known times equals hand-computed values and is a
 * pure function of time; velocity is consistent with the position.
 */
class NtnOrbitPositionTestCase : public TestCase
{
  public:
    NtnOrbitPositionTestCase()
        : TestCase("T3 M01-T01 deterministic satellite position and velocity")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<CircularOrbitMobilityModel> orbit = CreateObject<CircularOrbitMobilityModel>();
        const Time quarter = Seconds(orbit->GetOrbitPeriod().GetSeconds() / 4.0);

        // t = 0, phase 0: the satellite sits above the ascending node, on the x axis.
        const Vector p0 = orbit->GetPositionAt(Time(0));
        NS_TEST_ASSERT_MSG_EQ_TOL(p0.x, ORBIT_RADIUS_600_KM_M, POSITION_TOLERANCE_M, "x at t=0");
        NS_TEST_ASSERT_MSG_EQ_TOL(p0.y, 0.0, POSITION_TOLERANCE_M, "y at t=0");
        NS_TEST_ASSERT_MSG_EQ_TOL(p0.z, 0.0, POSITION_TOLERANCE_M, "z at t=0");

        // t = T/4, phase 90 deg: (0, a cos 53deg, a sin 53deg)
        //   = (0, 6971000 * 0.6018150, 6971000 * 0.7986355) = (0, 4195252.526, 5567288.141).
        const Vector pQ = orbit->GetPositionAt(quarter);
        NS_TEST_ASSERT_MSG_EQ_TOL(pQ.x, 0.0, POSITION_TOLERANCE_M, "x at T/4");
        NS_TEST_ASSERT_MSG_EQ_TOL(pQ.y, 4195252.526392929, POSITION_TOLERANCE_M, "y at T/4");
        NS_TEST_ASSERT_MSG_EQ_TOL(pQ.z, 5567288.140539679, POSITION_TOLERANCE_M, "z at T/4");

        // Pure function of time: same answer on repeated calls, and full period returns to start.
        const Vector again = orbit->GetPositionAt(quarter);
        NS_TEST_ASSERT_MSG_EQ(again, pQ, "GetPositionAt must be deterministic");
        const Vector pT = orbit->GetPositionAt(orbit->GetOrbitPeriod());
        NS_TEST_ASSERT_MSG_EQ_TOL(CalculateDistance(pT, p0), 0.0, 0.01, "position after T");

        // RAAN 90 deg turns the ring a quarter turn about the Earth axis: start at (0, a, 0).
        orbit->SetRaan(90.0);
        const Vector pRaan = orbit->GetPositionAt(Time(0));
        NS_TEST_ASSERT_MSG_EQ_TOL(pRaan.x, 0.0, POSITION_TOLERANCE_M, "x with RAAN 90");
        NS_TEST_ASSERT_MSG_EQ_TOL(pRaan.y, ORBIT_RADIUS_600_KM_M, POSITION_TOLERANCE_M, "y RAAN");
        orbit->SetRaan(0.0);

        // Velocity: speed a*w, perpendicular to the position, equal to the numeric derivative.
        const Time t = Seconds(1234.5);
        const Vector p = orbit->GetPositionAt(t);
        const Vector v = orbit->GetVelocityAt(t);
        NS_TEST_ASSERT_MSG_EQ_TOL(v.GetLength(), SPEED_600_KM_MPS, 0.001, "orbital speed");
        NS_TEST_ASSERT_MSG_EQ_TOL((p.x * v.x + p.y * v.y + p.z * v.z) /
                                      (p.GetLength() * v.GetLength()),
                                  0.0,
                                  1e-12,
                                  "velocity must be perpendicular to position");
        const Time h = MilliSeconds(1);
        const Vector numeric = orbit->GetPositionAt(t + h) - orbit->GetPositionAt(t - h);
        NS_TEST_ASSERT_MSG_EQ_TOL(numeric.x / (2 * h.GetSeconds()), v.x, 0.01, "dx/dt");
        NS_TEST_ASSERT_MSG_EQ_TOL(numeric.y / (2 * h.GetSeconds()), v.y, 0.01, "dy/dt");
        NS_TEST_ASSERT_MSG_EQ_TOL(numeric.z / (2 * h.GetSeconds()), v.z, 0.01, "dz/dt");

        // GetPosition() during a simulation returns GetPositionAt(Simulator::Now()).
        Vector simulated;
        Simulator::Schedule(t, [&simulated, orbit]() { simulated = orbit->GetPosition(); });
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(simulated, p, "GetPosition must equal GetPositionAt(now)");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T3: orbit period from T = 2*pi*sqrt(a^3/mu).
 */
class NtnOrbitPeriodTestCase : public TestCase
{
  public:
    NtnOrbitPeriodTestCase()
        : TestCase("T3 orbit period at 600 km is 5792.3 s (96.5 min)")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<CircularOrbitMobilityModel> orbit = CreateObject<CircularOrbitMobilityModel>();
        NS_TEST_ASSERT_MSG_EQ_TOL(orbit->GetOrbitPeriod().GetSeconds(),
                                  PERIOD_600_KM_S,
                                  1.0,
                                  "period at 600 km");
        NS_TEST_ASSERT_MSG_EQ_TOL(orbit->GetOrbitPeriod().GetMinutes(), 96.5, 0.1, "minutes");
        // 2000 km: 2*pi*sqrt(8371000^3 / mu) = 7622.141 s; higher orbits are slower.
        orbit->SetOrbitAltitude(2000e3);
        NS_TEST_ASSERT_MSG_EQ_TOL(orbit->GetOrbitPeriod().GetSeconds(), 7622.141, 1.0, "2000 km");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T3 / M01-T03: visibility flips exactly at the MinElevation boundary.
 */
class NtnVisibilityBoundaryTestCase : public TestCase
{
  public:
    NtnVisibilityBoundaryTestCase()
        : TestCase("T3 M01-T03 visibility changes at the elevation boundary")
    {
    }

  private:
    void DoRun() override
    {
        const Vector groundM(ntn::EARTH_RADIUS_M, 0.0, 0.0); // latitude 0, longitude 0
        // Straight up: 90 deg. Along the ground (tangent): 0 deg. Through the Earth: negative.
        NS_TEST_ASSERT_MSG_EQ_TOL(
            NtnVisibilityModel::ComputeElevationDeg(groundM, Vector(ORBIT_RADIUS_600_KM_M, 0, 0)),
            90.0,
            1e-9,
            "zenith");
        NS_TEST_ASSERT_MSG_EQ_TOL(
            NtnVisibilityModel::ComputeElevationDeg(groundM, Vector(ntn::EARTH_RADIUS_M, 1e6, 0)),
            0.0,
            1e-9,
            "horizon");
        NS_TEST_ASSERT_MSG_LT(
            NtnVisibilityModel::ComputeElevationDeg(groundM, Vector(-ORBIT_RADIUS_600_KM_M, 0, 0)),
            0.0,
            "far side of the Earth");

        Ptr<NtnVisibilityModel> visibility = CreateObject<NtnVisibilityModel>();
        NS_TEST_ASSERT_MSG_EQ(visibility->IsVisibleAtElevation(10.0), true, "exactly 10 deg");
        NS_TEST_ASSERT_MSG_EQ(visibility->IsVisibleAtElevation(9.999), false, "just below");

        // Equatorial orbit over a terminal at (0, 0): the Earth-centre angle between them is the
        // phase itself. At 15.836 deg the elevation is exactly 10 deg (see the constant).
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        const double margin = 0.01; // deg of orbit, about 1.2 km
        Ptr<Node> inside = CreateSatelliteNode(0.0, 360.0 - CENTRAL_ANGLE_600_KM_10_DEG + margin);
        Ptr<Node> outside = CreateSatelliteNode(0.0, 360.0 - CENTRAL_ANGLE_600_KM_10_DEG - margin);
        auto groundMobility = ground->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ_TOL(visibility->GetElevationDeg(
                                      groundMobility,
                                      CreateSatelliteNode(0.0, 360.0 - CENTRAL_ANGLE_600_KM_10_DEG)
                                          ->GetObject<MobilityModel>()),
                                  10.0,
                                  1e-6,
                                  "closed-form boundary angle gives 10 deg elevation");
        NS_TEST_ASSERT_MSG_EQ(
            visibility->IsVisible(groundMobility, inside->GetObject<MobilityModel>()),
            true,
            "just inside the boundary");
        NS_TEST_ASSERT_MSG_EQ(
            visibility->IsVisible(groundMobility, outside->GetObject<MobilityModel>()),
            false,
            "just outside the boundary");

        // Moving satellite: the link manager reports the flip within one update of the
        // predicted rise time t = (30 - 15.836) deg / w.
        Ptr<Node> moving = CreateSatelliteNode(0.0, 330.0);
        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        manager->SetAttribute("UpdateInterval", TimeValue(Seconds(1)));
        manager->AddGroundNode(ground);
        manager->AddSatelliteNode(moving);
        std::vector<std::pair<Time, bool>> changes;
        manager->TraceConnectWithoutContext(
            "VisibilityChange",
            MakeCallback(+[](std::vector<std::pair<Time, bool>>* c, uint32_t, uint32_t, bool v) {
                c->emplace_back(Simulator::Now(), v);
            }).Bind(&changes));
        const double omegaDegPerS = ntn::RadToDeg(
            moving->GetObject<CircularOrbitMobilityModel>()->GetAngularVelocityRadPerS());
        const double riseS = (30.0 - CENTRAL_ANGLE_600_KM_10_DEG) / omegaDegPerS;
        const double setS = (30.0 + CENTRAL_ANGLE_600_KM_10_DEG) / omegaDegPerS;
        manager->Start();
        Simulator::Stop(Seconds(1000));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(changes.size(), 2, "one rise and one set");
        NS_TEST_ASSERT_MSG_EQ(changes[0].second, true, "first change is a rise");
        NS_TEST_ASSERT_MSG_EQ(changes[1].second, false, "second change is a set");
        NS_TEST_ASSERT_MSG_EQ_TOL(changes[0].first.GetSeconds(), riseS, 1.0, "rise time");
        NS_TEST_ASSERT_MSG_EQ_TOL(changes[1].first.GetSeconds(), setS, 1.0, "set time");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T3 / M01-T04 / V1: delay = path length / propagation speed (+ processing) within 1 ns.
 */
class NtnDelayTestCase : public TestCase
{
  public:
    NtnDelayTestCase()
        : TestCase("T3 M01-T04 V1 propagation delay equals path length over c")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<NtnPropagationDelayModel> delay = CreateObject<NtnPropagationDelayModel>();
        for (double pathM : {0.0, 600e3, 1234567.8, 3e6})
        {
            NS_TEST_ASSERT_MSG_EQ_TOL(delay->GetPropagationDelay(pathM).GetSeconds(),
                                      pathM / 299792458.0,
                                      ONE_NS_S,
                                      "propagation delay for " << pathM << " m");
            NS_TEST_ASSERT_MSG_EQ_TOL(delay->GetLinkDelay(pathM).GetSeconds(),
                                      pathM / 299792458.0 + 1e-3,
                                      ONE_NS_S,
                                      "link delay for " << pathM << " m");
        }

        // V1: satellite straight overhead at 600 km -> 600,000 / 299,792,458 = 2.0013846 ms.
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        Ptr<Node> sat = CreateSatelliteNode(0.0, 0.0);
        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        manager->AddGroundNode(ground);
        manager->AddSatelliteNode(sat);
        manager->Update();
        const NtnPairState pair = manager->GetPairState(ground->GetId(), sat->GetId());
        NS_TEST_ASSERT_MSG_EQ_TOL(pair.slantRangeM, 600e3, POSITION_TOLERANCE_M, "overhead range");
        NS_TEST_ASSERT_MSG_EQ_TOL(pair.propagationDelay.GetSeconds(),
                                  2.0013845711889e-3,
                                  ONE_NS_S,
                                  "V1 overhead propagation delay");
        NS_TEST_ASSERT_MSG_EQ_TOL(pair.linkDelay.GetSeconds(),
                                  3.0013845711889e-3,
                                  ONE_NS_S,
                                  "V1 overhead link delay incl. 1 ms processing");
        const NtnLinkState link = manager->GetLinkState(ground->GetId(), sat->GetId());
        NS_TEST_ASSERT_MSG_EQ_TOL(link.propagationDelayMs, 2.0013845711889, 1e-6, "LinkState");
        NS_TEST_ASSERT_MSG_EQ(link.dataRateMbps, 100.0, "LinkState data rate");
        NS_TEST_ASSERT_MSG_EQ(link.available, true, "associated link is available");
        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T3 / M01-T05: the baseline policy picks the highest-elevation visible satellite.
 */
class NtnPolicyTestCase : public TestCase
{
  public:
    NtnPolicyTestCase()
        : TestCase("T3 M01-T05 highest-elevation policy picks the expected satellite")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<HighestElevationPolicy> policy = CreateObject<HighestElevationPolicy>();
        NtnAssociationState state;
        NS_TEST_ASSERT_MSG_EQ(policy->Select(state).satelliteId,
                              ntn::NO_SATELLITE,
                              "no candidates -> no association");
        state.visibleCandidates = {{5, 20.0, 1.5e6, Time(0)},
                                   {3, 60.0, 0.7e6, Time(0)},
                                   {9, 45.0, 0.9e6, Time(0)}};
        NS_TEST_ASSERT_MSG_EQ(policy->Select(state).satelliteId, 3, "highest elevation wins");
        state.visibleCandidates = {{7, 50.0, 1e6, Time(0)}, {4, 50.0, 1e6, Time(0)}};
        NS_TEST_ASSERT_MSG_EQ(policy->Select(state).satelliteId, 4, "tie -> lowest node ID");

        // Inside the link manager: two satellites, the one nearer the zenith must be chosen,
        // and with none visible the association is -1 without any error.
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        Ptr<Node> low = CreateSatelliteNode(0.0, 350.0);
        Ptr<Node> high = CreateSatelliteNode(0.0, 2.0);
        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        manager->AddGroundNode(ground);
        manager->AddSatelliteNode(low);
        manager->AddSatelliteNode(high);
        manager->Update();
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociatedSatellite(ground->GetId()),
                              static_cast<int32_t>(high->GetId()),
                              "satellite nearer the zenith");
        NS_TEST_ASSERT_MSG_EQ(manager->GetVisibleSatellites(ground->GetId()).size(),
                              2,
                              "both satellites are visible candidates (M01-FR-04)");

        Ptr<Node> farGround = CreateGroundNode(0.0, 180.0); // opposite side of the Earth
        manager->AddGroundNode(farGround);
        manager->Update();
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociatedSatellite(farGround->GetId()),
                              ntn::NO_SATELLITE,
                              "no satellite visible -> -1");
        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T4: every TraceSource fires with the right arguments during a scripted pass.
 */
class NtnTraceSourcesTestCase : public TestCase
{
  public:
    NtnTraceSourcesTestCase()
        : TestCase("T4 M01-FR-08 trace sources fire with correct arguments")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        Ptr<Node> sat = CreateSatelliteNode(0.0, 330.0);
        const uint32_t g = ground->GetId();
        const auto s = static_cast<int32_t>(sat->GetId());
        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        manager->SetAttribute("UpdateInterval", TimeValue(Seconds(1)));
        manager->AddGroundNode(ground);
        manager->AddSatelliteNode(sat);
        auto groundMobility = ground->GetObject<MobilityModel>();
        auto orbit = sat->GetObject<CircularOrbitMobilityModel>();

        m_visibility = 0;
        m_association.clear();
        m_rangeUpdates = 0;
        m_delayUpdates = 0;
        m_maxRangeErrorM = 0.0;
        m_maxDelayErrorS = 0.0;

        manager->TraceConnectWithoutContext(
            "VisibilityChange",
            MakeCallback(&NtnTraceSourcesTestCase::OnVisibility, this));
        manager->TraceConnectWithoutContext(
            "AssociationChange",
            MakeCallback(&NtnTraceSourcesTestCase::OnAssociation, this));
        manager->TraceConnectWithoutContext("SlantRangeUpdate",
                                            MakeCallback(&NtnTraceSourcesTestCase::OnRange, this));
        manager->TraceConnectWithoutContext("LinkDelayUpdate",
                                            MakeCallback(&NtnTraceSourcesTestCase::OnDelay, this));
        m_groundPositionM = groundMobility->GetPosition();
        m_orbit = orbit;

        manager->Start();
        Simulator::Stop(Seconds(1000));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_visibility, 2, "VisibilityChange: rise and set");
        NS_TEST_ASSERT_MSG_EQ(m_association.size(), 2, "AssociationChange: acquire and lose");
        NS_TEST_ASSERT_MSG_EQ(m_association[0].first, ntn::NO_SATELLITE, "acquire: old = -1");
        NS_TEST_ASSERT_MSG_EQ(m_association[0].second, s, "acquire: new = satellite");
        NS_TEST_ASSERT_MSG_EQ(m_association[1].first, s, "lose: old = satellite");
        NS_TEST_ASSERT_MSG_EQ(m_association[1].second, ntn::NO_SATELLITE, "lose: new = -1");
        NS_TEST_ASSERT_MSG_GT(m_rangeUpdates, 0, "SlantRangeUpdate fired");
        NS_TEST_ASSERT_MSG_EQ(m_rangeUpdates, m_delayUpdates, "one delay per range update");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_maxRangeErrorM, 0.0, POSITION_TOLERANCE_M, "range argument");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_maxDelayErrorS, 0.0, ONE_NS_S, "delay argument");
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociationChangeCount(g), 2, "change counter");
    }

    /**
     * VisibilityChange sink.
     * @param groundId ground ID
     * @param satelliteId satellite ID
     * @param visible new state
     */
    void OnVisibility(uint32_t groundId, uint32_t satelliteId, bool visible)
    {
        ++m_visibility;
    }

    /**
     * AssociationChange sink.
     * @param groundId ground ID
     * @param oldId previous satellite
     * @param newId new satellite
     */
    void OnAssociation(uint32_t groundId, int32_t oldId, int32_t newId)
    {
        m_association.emplace_back(oldId, newId);
    }

    /**
     * SlantRangeUpdate sink: compares the argument with an independent geometry calculation.
     * @param groundId ground ID
     * @param satelliteId satellite ID
     * @param rangeM reported range [m]
     */
    void OnRange(uint32_t groundId, uint32_t satelliteId, double rangeM)
    {
        ++m_rangeUpdates;
        const double expectedM =
            CalculateDistance(m_groundPositionM, m_orbit->GetPositionAt(Simulator::Now()));
        m_maxRangeErrorM = std::max(m_maxRangeErrorM, std::abs(rangeM - expectedM));
    }

    /**
     * LinkDelayUpdate sink: compares the argument with range / c + 1 ms.
     * @param groundId ground ID
     * @param satelliteId satellite ID
     * @param delay reported delay
     */
    void OnDelay(uint32_t groundId, uint32_t satelliteId, Time delay)
    {
        ++m_delayUpdates;
        const double rangeM =
            CalculateDistance(m_groundPositionM, m_orbit->GetPositionAt(Simulator::Now()));
        const double expectedS = rangeM / ntn::SPEED_OF_LIGHT_M_PER_S + 1e-3;
        m_maxDelayErrorS = std::max(m_maxDelayErrorS, std::abs(delay.GetSeconds() - expectedS));
    }

    uint32_t m_visibility{0};                               //!< VisibilityChange count
    std::vector<std::pair<int32_t, int32_t>> m_association; //!< AssociationChange arguments
    uint32_t m_rangeUpdates{0};                             //!< SlantRangeUpdate count
    uint32_t m_delayUpdates{0};                             //!< LinkDelayUpdate count
    double m_maxRangeErrorM{0.0};                           //!< Largest range argument error
    double m_maxDelayErrorS{0.0};                           //!< Largest delay argument error
    Vector m_groundPositionM;                               //!< Ground position
    Ptr<CircularOrbitMobilityModel> m_orbit;                //!< Satellite orbit
};

/**
 * @ingroup futurenet-ntn-tests
 * T5 / M01-T06: same configuration (and seed/run) -> byte-identical CSV output.
 */
class NtnReproducibilityTestCase : public TestCase
{
  public:
    NtnReproducibilityTestCase()
        : TestCase("T5 M01-T06 repeated runs give identical CSV output")
    {
    }

  private:
    /**
     * Run a 3-terminal / 6-satellite scenario and write both CSV files.
     * @param linkStatePath link-state CSV path
     * @param eventLogPath event log path
     */
    static void RunScenario(const std::string& linkStatePath, const std::string& eventLogPath)
    {
        NodeContainer sats;
        sats.Create(6);
        NodeContainer grounds;
        grounds.Create(3);
        NtnHelper ntn;
        ntn.SetLinkAttribute("UpdateInterval", TimeValue(Seconds(1)));
        ntn.InstallSatellites(sats);
        for (uint32_t k = 0; k < grounds.GetN(); ++k)
        {
            const auto [lat, lon] = NtnHelper::GetGroundTrackPoint(k * 40.0, 1.0, 53.0, 0.0);
            ntn.InstallGroundTerminal(grounds.Get(k), lat, lon);
        }
        ntn.EnableTracing(linkStatePath, eventLogPath, Seconds(10));
        ntn.Start();
        Simulator::Stop(Seconds(1500));
        Simulator::Run();
        Simulator::Destroy();
    }

    void DoRun() override
    {
        RngSeedManager::SetSeed(1);
        RngSeedManager::SetRun(1);
        const std::string linkA = CreateTempDirFilename("link-a.csv");
        const std::string eventA = CreateTempDirFilename("event-a.csv");
        const std::string linkB = CreateTempDirFilename("link-b.csv");
        const std::string eventB = CreateTempDirFilename("event-b.csv");
        RunScenario(linkA, eventA);
        RunScenario(linkB, eventB);

        const std::string firstLink = ReadFile(linkA);
        const std::string firstEvents = ReadFile(eventA);
        NS_TEST_ASSERT_MSG_GT(firstLink.size(), 100, "link-state CSV has content");
        NS_TEST_ASSERT_MSG_NE(firstEvents.find("AssociationChange"),
                              std::string::npos,
                              "event log has association events");
        NS_TEST_ASSERT_MSG_EQ(firstLink, ReadFile(linkB), "link-state CSV must be identical");
        NS_TEST_ASSERT_MSG_EQ(firstEvents, ReadFile(eventB), "event log must be identical");
        NS_TEST_ASSERT_MSG_EQ(firstLink.substr(0, firstLink.find('\n')),
                              "time_s,ground_id,satellite_id,visible,elevation_deg,range_km,"
                              "delay_ms,associated",
                              "SRS section 9 CSV header");
        NS_TEST_ASSERT_MSG_EQ(firstEvents.substr(0, firstEvents.find('\n')),
                              "time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,"
                              "detail",
                              "Common Guidelines section 4.5 event-log header");
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T6 / M01-FR-09: real packets over the ns-3 Internet stack and PointToPoint links experience
 * exactly the computed link delay plus the transmission time at the configured DataRate.
 */
class NtnIntegrationTestCase : public TestCase
{
  public:
    NtnIntegrationTestCase()
        : TestCase("T6 M01-FR-09 packets over IP see the computed delay and data rate")
    {
    }

  private:
    /**
     * Receive handler of the satellite socket.
     * @param socket the socket
     */
    void Receive(Ptr<Socket> socket)
    {
        while (socket->Recv())
        {
            m_rxTimes.push_back(Simulator::Now());
        }
    }

    /**
     * Send one packet from the ground terminal and record the expected arrival time.
     * @param socket ground socket
     * @param destination satellite service address and port
     */
    void Send(Ptr<Socket> socket, InetSocketAddress destination)
    {
        const NtnPairState pair = m_manager->GetPairState(m_groundId, m_satelliteId);
        // On-the-wire size: payload + UDP (8) + IPv4 (20) + PPP (2) bytes.
        const uint32_t wireBytes = PAYLOAD_BYTES + 8 + 20 + 2;
        const Time txTime = m_dataRate.CalculateBytesTxTime(wireBytes);
        m_expectedRxTimes.push_back(Simulator::Now() + txTime + pair.linkDelay);
        m_channelDelays.push_back(
            m_helper->GetIpLinkController()->GetChannelDelay(m_groundId, m_satelliteId));
        m_pairDelays.push_back(pair.linkDelay);
        socket->SendTo(Create<Packet>(PAYLOAD_BYTES), 0, destination);
    }

    void DoRun() override
    {
        NodeContainer sats;
        sats.Create(1);
        NodeContainer grounds;
        grounds.Create(1);
        NtnHelper helper;
        m_helper = &helper;
        m_dataRate = DataRate("50Mbps");
        helper.SetLinkAttribute("DataRate", DataRateValue(m_dataRate));
        helper.InstallSatellites(sats); // phase 0, inclination 53: overhead of (0, 0) at t=0
        helper.InstallGroundTerminal(grounds.Get(0), 0.0, 0.0);
        InternetStackHelper internet;
        internet.Install(sats);
        internet.Install(grounds);
        Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
        helper.InstallIpLinks(addresses);
        helper.Start();
        m_manager = helper.GetLinkManager();
        m_groundId = grounds.Get(0)->GetId();
        m_satelliteId = sats.Get(0)->GetId();

        const uint16_t port = 9;
        Ptr<Socket> rx = Socket::CreateSocket(sats.Get(0), UdpSocketFactory::GetTypeId());
        rx->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        rx->SetRecvCallback(MakeCallback(&NtnIntegrationTestCase::Receive, this));
        Ptr<Socket> tx = Socket::CreateSocket(grounds.Get(0), UdpSocketFactory::GetTypeId());
        const InetSocketAddress destination(helper.GetSatelliteServiceAddress(), port);

        // Send between updates (updates every 100 ms), at different satellite positions.
        for (double sendS : {0.55, 30.05, 120.05})
        {
            Simulator::Schedule(Seconds(sendS),
                                &NtnIntegrationTestCase::Send,
                                this,
                                tx,
                                destination);
        }
        Simulator::Stop(Seconds(121));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(helper.GetIpLinkController()->IsLinkUp(m_groundId, m_satelliteId),
                              true,
                              "associated link is up");
        NS_TEST_ASSERT_MSG_EQ(m_rxTimes.size(), m_expectedRxTimes.size(), "all packets arrive");
        for (std::size_t i = 0; i < m_rxTimes.size(); ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(m_channelDelays[i],
                                  m_pairDelays[i],
                                  "channel Delay follows the computed link delay");
            NS_TEST_ASSERT_MSG_EQ_TOL(m_rxTimes[i].GetSeconds(),
                                      m_expectedRxTimes[i].GetSeconds(),
                                      ONE_NS_S,
                                      "measured one-way delay = link delay + transmission time");
        }
        NS_TEST_ASSERT_MSG_NE(m_pairDelays.front(), m_pairDelays.back(), "delay changes in time");
        Simulator::Destroy();
        m_helper = nullptr;
    }

    static constexpr uint32_t PAYLOAD_BYTES = 100; //!< UDP payload size [bytes]
    NtnHelper* m_helper{nullptr};                  //!< Helper of the running scenario
    Ptr<NtnLinkManager> m_manager;                 //!< Link manager
    uint32_t m_groundId{0};                        //!< Ground node ID
    uint32_t m_satelliteId{0};                     //!< Satellite node ID
    DataRate m_dataRate;                           //!< Link data rate
    std::vector<Time> m_rxTimes;                   //!< Packet arrival times
    std::vector<Time> m_expectedRxTimes;           //!< Expected arrival times
    std::vector<Time> m_channelDelays;             //!< Channel Delay at send time
    std::vector<Time> m_pairDelays;                //!< Computed link delay at send time
};

/**
 * @ingroup futurenet-ntn-tests
 * V2 / V3 / AC-05: range falls then rises during a pass; in a multi-satellite run every
 * association goes to a visible satellite and at least one handover happens.
 */
class NtnValidationScenarioTestCase : public TestCase
{
  public:
    NtnValidationScenarioTestCase()
        : TestCase("V2 V3 AC-05 single pass shape and association only to visible satellites")
    {
    }

  private:
    void DoRun() override
    {
        // V2: ranges sampled every 10 s over one pass, closest point in the middle.
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        Ptr<Node> sat = CreateSatelliteNode(0.0, 330.0);
        auto groundMobility = ground->GetObject<MobilityModel>();
        auto orbit = sat->GetObject<CircularOrbitMobilityModel>();
        std::vector<double> rangesM;
        for (int t = 0; t <= 1000; t += 10)
        {
            rangesM.push_back(
                CalculateDistance(groundMobility->GetPosition(), orbit->GetPositionAt(Seconds(t))));
        }
        const auto closest = std::min_element(rangesM.begin(), rangesM.end());
        NS_TEST_ASSERT_MSG_EQ(std::is_sorted(rangesM.begin(), closest + 1, std::greater<>()),
                              true,
                              "range decreases before the closest point");
        NS_TEST_ASSERT_MSG_EQ(std::is_sorted(closest, rangesM.end()),
                              true,
                              "range increases after the closest point");
        NS_TEST_ASSERT_MSG_EQ_TOL(*closest, 600e3, 20e3, "closest point is nearly overhead");

        // V3 + AC-05: 4 terminals on the track of 12 satellites, 20 minutes.
        NodeContainer sats;
        sats.Create(12);
        NodeContainer grounds;
        grounds.Create(4);
        NtnHelper ntn;
        ntn.InstallSatellites(sats);
        for (uint32_t k = 0; k < grounds.GetN(); ++k)
        {
            const auto [lat, lon] = NtnHelper::GetGroundTrackPoint(k * 90.0, 0.0, 53.0, 0.0);
            ntn.InstallGroundTerminal(grounds.Get(k), lat, lon);
        }
        Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
        m_manager = manager;
        m_handovers = 0;
        m_invisibleAssociations = 0;
        manager->TraceConnectWithoutContext(
            "AssociationChange",
            MakeCallback(&NtnValidationScenarioTestCase::OnAssociation, this));
        ntn.Start();
        Simulator::Stop(Seconds(1200));
        Simulator::Run();
        Simulator::Destroy();
        m_manager = nullptr;

        NS_TEST_ASSERT_MSG_GT(m_handovers,
                              0,
                              "AC-05: at least one satellite-to-satellite handover");
        NS_TEST_ASSERT_MSG_EQ(m_invisibleAssociations,
                              0,
                              "V3: never associated to an invisible satellite");
    }

    /**
     * AssociationChange sink: the new satellite must be visible at this instant.
     * @param groundId ground ID
     * @param oldId previous satellite
     * @param newId new satellite
     */
    void OnAssociation(uint32_t groundId, int32_t oldId, int32_t newId)
    {
        if (newId == ntn::NO_SATELLITE)
        {
            return;
        }
        if (oldId != ntn::NO_SATELLITE)
        {
            ++m_handovers;
        }
        if (!m_manager->GetPairState(groundId, static_cast<uint32_t>(newId)).visible)
        {
            ++m_invisibleAssociations;
        }
    }

    Ptr<NtnLinkManager> m_manager;       //!< Link manager of the running scenario
    uint32_t m_handovers{0};             //!< Satellite-to-satellite handovers seen
    uint32_t m_invisibleAssociations{0}; //!< Associations to a non-visible satellite
};

/**
 * @ingroup futurenet-ntn-tests
 * T1 / T2 / T3 for M01-FR-10: Walker layout defaults, rejected values, and satellite angles and
 * positions in a multi-plane constellation (expected values worked out by hand from the
 * NtnWalkerConstellation formulas).
 */
class NtnWalkerConstellationTestCase : public TestCase
{
  public:
    NtnWalkerConstellationTestCase()
        : TestCase("T1 T2 T3 M01-FR-10 multi-plane Walker constellation layout")
    {
    }

  private:
    void DoRun() override
    {
        // T1: defaults keep the single-plane layout of Phase 1.
        Ptr<NtnWalkerConstellation> walker = CreateObject<NtnWalkerConstellation>();
        UintegerValue planes;
        DoubleValue d;
        walker->GetAttribute("NumOrbitalPlanes", planes);
        NS_TEST_ASSERT_MSG_EQ(planes.Get(), 1, "NumOrbitalPlanes default must be 1 (SRS)");
        walker->GetAttribute("PlanePhaseOffset", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 0.0, "PlanePhaseOffset default must be 0 deg");
        walker->GetAttribute("RaanSpread", d);
        NS_TEST_ASSERT_MSG_EQ(d.Get(), 360.0, "RaanSpread default must be 360 deg");

        // T2: out-of-range values are rejected.
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("NumOrbitalPlanes", UintegerValue(0)),
                              false,
                              "0 planes rejected");
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("NumOrbitalPlanes", UintegerValue(21)),
                              false,
                              "21 planes rejected");
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("NumOrbitalPlanes", UintegerValue(20)),
                              true,
                              "20 planes accepted");
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("PlanePhaseOffset", DoubleValue(360.0)),
                              false,
                              "PlanePhaseOffset 360 rejected");
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("RaanSpread", DoubleValue(0.0)),
                              false,
                              "RaanSpread 0 rejected");
        NS_TEST_EXPECT_MSG_EQ(walker->SetAttributeFailSafe("RaanSpread", DoubleValue(360.1)),
                              false,
                              "RaanSpread 360.1 rejected");

        // T3: 6 satellites, 3 planes, offset 20 deg, Raan0 = 10, phase0 = 5. Two satellites per
        // plane, 180 deg apart; planes 120 deg apart; each plane shifted 20 deg forward.
        walker->SetNumOrbitalPlanes(3);
        walker->SetPlanePhaseOffset(20.0);
        walker->SetRaanSpread(360.0);
        const double expected[6][4] = {{0, 0, 10, 5},
                                       {0, 1, 10, 185},
                                       {1, 0, 130, 25},
                                       {1, 1, 130, 205},
                                       {2, 0, 250, 45},
                                       {2, 1, 250, 225}};
        for (uint32_t k = 0; k < 6; ++k)
        {
            const NtnOrbitSlot slot = walker->GetSlot(k, 6, 10.0, 5.0);
            NS_TEST_ASSERT_MSG_EQ(slot.planeIndex, expected[k][0], "plane of satellite " << k);
            NS_TEST_ASSERT_MSG_EQ(slot.slotInPlane, expected[k][1], "slot of satellite " << k);
            NS_TEST_ASSERT_MSG_EQ_TOL(slot.raanDeg, expected[k][2], 1e-9, "RAAN of " << k);
            NS_TEST_ASSERT_MSG_EQ_TOL(slot.initialPhaseDeg, expected[k][3], 1e-9, "phase " << k);
        }
        // Angles wrap into [0, 360): 300 + 2 * 120 = 540 -> 180.
        NS_TEST_ASSERT_MSG_EQ_TOL(walker->GetSlot(4, 6, 300.0, 0.0).raanDeg,
                                  180.0,
                                  1e-9,
                                  "RAAN wraps around");
        // Walker star: planes spread over 180 deg -> RAAN 0, 60, 120.
        walker->SetRaanSpread(180.0);
        NS_TEST_ASSERT_MSG_EQ_TOL(walker->GetSlot(5, 6, 0.0, 0.0).raanDeg,
                                  120.0,
                                  1e-9,
                                  "star layout");

        // Through the helper: 6 satellites in 3 planes. At t = 0 a satellite with phase u = 0
        // sits on its ascending node, i.e. on the equator at longitude = RAAN, whatever the
        // inclination: position (a cos RAAN, a sin RAAN, 0).
        NodeContainer sats;
        sats.Create(6);
        NtnHelper ntn;
        ntn.SetConstellationAttribute("NumOrbitalPlanes", UintegerValue(3));
        ntn.InstallSatellites(sats);
        for (uint32_t k = 0; k < 6; k += 2)
        {
            const NtnOrbitSlot slot = ntn.GetOrbitSlot(sats.Get(k)->GetId());
            const double raanRad = ntn::DegToRad(k / 2 * 120.0);
            const Vector position = sats.Get(k)->GetObject<MobilityModel>()->GetPosition();
            NS_TEST_ASSERT_MSG_EQ(slot.planeIndex, k / 2, "helper plane index");
            NS_TEST_ASSERT_MSG_EQ_TOL(position.x,
                                      ORBIT_RADIUS_600_KM_M * std::cos(raanRad),
                                      POSITION_TOLERANCE_M,
                                      "x of first satellite in plane " << k / 2);
            NS_TEST_ASSERT_MSG_EQ_TOL(position.y,
                                      ORBIT_RADIUS_600_KM_M * std::sin(raanRad),
                                      POSITION_TOLERANCE_M,
                                      "y of first satellite in plane " << k / 2);
            NS_TEST_ASSERT_MSG_EQ_TOL(position.z, 0.0, POSITION_TOLERANCE_M, "on the equator");
        }
        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-ntn-tests
 * T1 / T2 / T3 / T4 for MaxAssociationsPerGround: default 1, range 1 - 4, the policy fills the
 * extra associations by elevation, the trace fires for added/removed secondary satellites, and
 * the IP links stay consistent with the association set at every update.
 */
class NtnMultiAssociationTestCase : public TestCase
{
  public:
    NtnMultiAssociationTestCase()
        : TestCase("T1 T2 T3 T4 MaxAssociationsPerGround keeps up to 4 satellites associated")
    {
    }

  private:
    void DoRun() override
    {
        // T1 / T2
        Ptr<NtnLinkManager> manager = CreateObject<NtnLinkManager>();
        UintegerValue max;
        manager->GetAttribute("MaxAssociationsPerGround", max);
        NS_TEST_ASSERT_MSG_EQ(max.Get(), 1, "MaxAssociationsPerGround default must be 1 (SRS)");
        NS_TEST_EXPECT_MSG_EQ(
            manager->SetAttributeFailSafe("MaxAssociationsPerGround", UintegerValue(0)),
            false,
            "0 rejected");
        NS_TEST_EXPECT_MSG_EQ(
            manager->SetAttributeFailSafe("MaxAssociationsPerGround", UintegerValue(5)),
            false,
            "5 rejected");
        NS_TEST_EXPECT_MSG_EQ(
            manager->SetAttributeFailSafe("MaxAssociationsPerGround", UintegerValue(4)),
            true,
            "4 accepted");

        // T3 policy: 4 candidates, 3 allowed -> primary = highest, secondary = next two.
        Ptr<HighestElevationPolicy> policy = CreateObject<HighestElevationPolicy>();
        NtnAssociationState state;
        state.maxAssociations = 3;
        state.visibleCandidates = {{5, 20.0, 1.5e6, Time(0)},
                                   {3, 60.0, 0.7e6, Time(0)},
                                   {9, 45.0, 0.9e6, Time(0)},
                                   {2, 30.0, 1.1e6, Time(0)}};
        NtnAssociationDecision decision = policy->Select(state);
        NS_TEST_ASSERT_MSG_EQ(decision.satelliteId, 3, "primary = highest elevation");
        NS_TEST_ASSERT_MSG_EQ(decision.secondarySatelliteIds.size(), 2, "two secondary");
        NS_TEST_ASSERT_MSG_EQ(decision.secondarySatelliteIds[0], 9, "second highest");
        NS_TEST_ASSERT_MSG_EQ(decision.secondarySatelliteIds[1], 2, "third highest");
        state.maxAssociations = 1;
        NS_TEST_ASSERT_MSG_EQ(policy->Select(state).secondarySatelliteIds.size(),
                              0,
                              "max 1 -> no secondary (Phase 1 behaviour)");

        // T3 / T4 in the manager: equatorial satellites 2, 8 and 10 deg away from a terminal at
        // (0, 0) are all visible (limit 15.8 deg at 600 km / 10 deg elevation).
        Ptr<Node> ground = CreateGroundNode(0.0, 0.0);
        Ptr<Node> near = CreateSatelliteNode(0.0, 2.0);
        Ptr<Node> middle = CreateSatelliteNode(0.0, 8.0);
        Ptr<Node> far = CreateSatelliteNode(0.0, 350.0);
        const uint32_t g = ground->GetId();
        manager = CreateObject<NtnLinkManager>();
        manager->SetMaxAssociationsPerGround(2);
        manager->AddGroundNode(ground);
        manager->AddSatelliteNode(near);
        manager->AddSatelliteNode(middle);
        manager->AddSatelliteNode(far);
        m_secondary.clear();
        manager->TraceConnectWithoutContext(
            "SecondaryAssociationChange",
            MakeCallback(&NtnMultiAssociationTestCase::OnSecondary, this));
        manager->Update();
        const std::vector<uint32_t> associated = manager->GetAssociatedSatellites(g);
        NS_TEST_ASSERT_MSG_EQ(associated.size(), 2, "two associations");
        NS_TEST_ASSERT_MSG_EQ(associated[0], near->GetId(), "primary first");
        NS_TEST_ASSERT_MSG_EQ(associated[1], middle->GetId(), "then the secondary");
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociatedSatellite(g),
                              static_cast<int32_t>(near->GetId()),
                              "GetAssociatedSatellite returns the primary");
        NS_TEST_ASSERT_MSG_EQ(manager->GetLinkState(g, middle->GetId()).available,
                              true,
                              "secondary link is available");
        NS_TEST_ASSERT_MSG_EQ(manager->GetLinkState(g, far->GetId()).available,
                              false,
                              "third satellite is not associated");
        NS_TEST_ASSERT_MSG_EQ(m_secondary.size(), 1, "one secondary added");
        NS_TEST_ASSERT_MSG_EQ(m_secondary[0].first, middle->GetId(), "trace satellite ID");
        NS_TEST_ASSERT_MSG_EQ(m_secondary[0].second, true, "trace: added");
        manager->Update();
        NS_TEST_ASSERT_MSG_EQ(m_secondary.size(), 1, "no change -> no trace");
        manager->SetMaxAssociationsPerGround(1);
        manager->Update();
        NS_TEST_ASSERT_MSG_EQ(m_secondary.size(), 2, "secondary removed");
        NS_TEST_ASSERT_MSG_EQ(m_secondary[1].second, false, "trace: removed");
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociatedSatellites(g).size(), 1, "back to one");
        NS_TEST_ASSERT_MSG_EQ(manager->GetAssociationChangeCount(g),
                              1,
                              "secondary changes are not primary association changes");
        Simulator::Destroy();

        // IP links follow the association set: 4 terminals, 24 satellites (15 deg apart, so 2 or
        // 3 are in view at any time), up to 3 links each.
        NodeContainer sats;
        sats.Create(24);
        NodeContainer grounds;
        grounds.Create(4);
        NtnHelper ntn;
        ntn.SetLinkAttribute("MaxAssociationsPerGround", UintegerValue(3));
        ntn.SetLinkAttribute("UpdateInterval", TimeValue(Seconds(1)));
        ntn.InstallSatellites(sats);
        for (uint32_t k = 0; k < grounds.GetN(); ++k)
        {
            const auto [lat, lon] = NtnHelper::GetGroundTrackPoint(k * 90.0, 1.0, 53.0, 0.0);
            ntn.InstallGroundTerminal(grounds.Get(k), lat, lon);
        }
        InternetStackHelper internet;
        internet.Install(sats);
        internet.Install(grounds);
        Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
        ntn.InstallIpLinks(addresses);
        m_helper = &ntn;
        m_inconsistentLinks = 0;
        m_maxAssociated = 0;
        m_secondary.clear();
        ntn.GetLinkManager()->TraceConnectWithoutContext(
            "UpdateCompleted",
            MakeCallback(&NtnMultiAssociationTestCase::OnUpdate, this));
        ntn.GetLinkManager()->TraceConnectWithoutContext(
            "SecondaryAssociationChange",
            MakeCallback(&NtnMultiAssociationTestCase::OnSecondary, this));
        ntn.Start();
        Simulator::Stop(Seconds(1200));
        Simulator::Run();
        Simulator::Destroy();
        m_helper = nullptr;

        NS_TEST_ASSERT_MSG_EQ(m_inconsistentLinks, 0, "link up <=> pair associated");
        NS_TEST_ASSERT_MSG_EQ(m_maxAssociated, 3, "terminals reach 3 associations");
        NS_TEST_ASSERT_MSG_GT(m_secondary.size(), 0, "secondary set changes over time");
    }

    /**
     * SecondaryAssociationChange sink.
     * @param groundId ground ID
     * @param satelliteId satellite ID
     * @param associated added or removed
     */
    void OnSecondary(uint32_t groundId, uint32_t satelliteId, bool associated)
    {
        m_secondary.emplace_back(satelliteId, associated);
    }

    /**
     * UpdateCompleted sink: every IP link must be up exactly when its pair is associated, and
     * no terminal may exceed MaxAssociationsPerGround.
     * @param now update time
     */
    void OnUpdate(Time now)
    {
        Ptr<NtnLinkManager> manager = m_helper->GetLinkManager();
        Ptr<NtnIpLinkController> controller = m_helper->GetIpLinkController();
        for (const auto& pair : manager->GetPairStates())
        {
            if (controller->IsLinkUp(pair.groundId, pair.satelliteId) != pair.associated)
            {
                ++m_inconsistentLinks;
            }
        }
        const NodeContainer grounds = manager->GetGroundNodes();
        for (auto it = grounds.Begin(); it != grounds.End(); ++it)
        {
            const auto n =
                static_cast<uint32_t>(manager->GetAssociatedSatellites((*it)->GetId()).size());
            m_maxAssociated = std::max(m_maxAssociated, n);
        }
    }

    std::vector<std::pair<uint32_t, bool>> m_secondary; //!< SecondaryAssociationChange arguments
    NtnHelper* m_helper{nullptr};                       //!< Helper of the running scenario
    uint32_t m_inconsistentLinks{0};                    //!< Link state != association state
    uint32_t m_maxAssociated{0};                        //!< Most associations seen at once
};

/**
 * @ingroup futurenet-ntn-tests
 * The futurenet-ntn test suite.
 */
class FuturenetNtnTestSuite : public TestSuite
{
  public:
    FuturenetNtnTestSuite()
        : TestSuite("futurenet-ntn", Type::UNIT)
    {
        AddTestCase(new NtnDefaultsTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnAttributeValidationTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnOrbitPositionTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnOrbitPeriodTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnVisibilityBoundaryTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnDelayTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnPolicyTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnTraceSourcesTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnReproducibilityTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnIntegrationTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnValidationScenarioTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnWalkerConstellationTestCase, TestCase::Duration::QUICK);
        AddTestCase(new NtnMultiAssociationTestCase, TestCase::Duration::QUICK);
    }
};

/// Static instance that registers the suite with test.py.
static FuturenetNtnTestSuite g_futurenetNtnTestSuite;
