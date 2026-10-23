/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn
 * Validation experiments (Common Guidelines section 10, SRS section 12).
 *
 * Each case writes a small CSV into --outputDir, prints PASS/FAIL, and compares the simulated
 * values against closed-form results where one exists:
 *
 * - V1 (baseline): satellite straight overhead -> delay = altitude / c + processing delay.
 * - V2 (one parameter varied): overhead delay for altitudes 200, 600, 1200, 2000 km must grow
 *   with altitude; plus SRS V2: during one pass the slant range falls, then rises.
 * - V3 (boundary): MinElevation 0 and 45 deg at altitudes 200 and 2000 km; the simulated
 *   visibility duration and largest visible range are compared to closed form; plus SRS V3:
 *   in a 20-terminal / 12-satellite run every association goes to a visible satellite.
 * - V4 (policy comparison, Phase 2): the same Walker constellation and terminals run once with
 *   HighestElevationPolicy and once with StickyAssociationPolicy. Expected: the sticky policy
 *   makes fewer handovers, the baseline has the lower mean delay (at one altitude the highest
 *   satellite is always the nearest), and both give the same coverage.
 *
 * Run: ./ns3 run "ntn-validation --outputDir=contrib/futurenet-ntn/doc/validation"
 */

#include "ns3/core-module.h"
#include "ns3/futurenet-ntn-module.h"
#include "ns3/network-module.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NtnValidation");

namespace
{

constexpr double M_PER_KM = 1e3;           ///< Metres per kilometre
constexpr double MS_PER_S = 1e3;           ///< Milliseconds per second
constexpr double NS_PER_S = 1e9;           ///< Nanoseconds per second
constexpr double DELAY_TOLERANCE_NS = 1.0; ///< V1 tolerance on the delay [ns]

/**
 * Closed-form slant range at a given elevation (law of cosines in the Earth-centre /
 * ground / satellite triangle): rho = sqrt(a^2 - R^2 cos^2 e) - R sin e.
 * @param altitudeM orbit altitude [m]
 * @param elevationDeg elevation [deg]
 * @return slant range [m]
 */
double
ClosedFormRangeM(double altitudeM, double elevationDeg)
{
    const double r = ntn::EARTH_RADIUS_M;
    const double a = r + altitudeM;
    const double e = ntn::DegToRad(elevationDeg);
    return std::sqrt(a * a - r * r * std::cos(e) * std::cos(e)) - r * std::sin(e);
}

/**
 * Closed-form Earth-centre angle between a ground point and a satellite seen at elevation e:
 * lambda = acos(R cos e / a) - e.
 * @param altitudeM orbit altitude [m]
 * @param elevationDeg elevation [deg]
 * @return central angle [deg]
 */
double
ClosedFormCentralAngleDeg(double altitudeM, double elevationDeg)
{
    const double r = ntn::EARTH_RADIUS_M;
    const double a = r + altitudeM;
    const double e = ntn::DegToRad(elevationDeg);
    return ntn::RadToDeg(std::acos(r * std::cos(e) / a) - e);
}

/**
 * Print and return a PASS/FAIL verdict.
 * @param name case name
 * @param ok verdict
 * @return ok
 */
bool
Verdict(const std::string& name, bool ok)
{
    std::cout << (ok ? "PASS " : "FAIL ") << name << "\n";
    return ok;
}

/**
 * Overhead geometry: one terminal at latitude/longitude 0 and one satellite exactly above it
 * at t = 0 (equatorial orbit, phase 0). Returns the pair state after one update.
 * @param altitudeM orbit altitude [m]
 * @return the pair state
 */
NtnPairState
OverheadState(double altitudeM)
{
    NodeContainer sat;
    sat.Create(1);
    NodeContainer ground;
    ground.Create(1);
    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeM));
    ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(0.0));
    ntn.InstallSatellites(sat);
    ntn.InstallGroundTerminal(ground.Get(0), 0.0, 0.0);
    ntn.GetLinkManager()->Update();
    const NtnPairState state =
        ntn.GetLinkManager()->GetPairState(ground.Get(0)->GetId(), sat.Get(0)->GetId());
    Simulator::Destroy();
    return state;
}

/**
 * V1: overhead delay at 600 km against the closed form.
 * @param dir output directory
 * @return true on PASS
 */
bool
RunV1(const std::string& dir)
{
    const double altitudeM = ntn::DEFAULT_ORBIT_ALTITUDE_M;
    const NtnPairState s = OverheadState(altitudeM);
    const double closedFormMs =
        altitudeM / ntn::SPEED_OF_LIGHT_M_PER_S * MS_PER_S + ntn::DEFAULT_PROCESSING_DELAY_MS;
    const double simMs = s.linkDelay.GetSeconds() * MS_PER_S;
    const double errorNs = std::abs(simMs - closedFormMs) / MS_PER_S * NS_PER_S;

    std::ofstream out(dir + "/v1-overhead-delay.csv");
    out << std::fixed << std::setprecision(9)
        << "altitude_km,range_km,propagation_delay_ms,sim_link_delay_ms,closed_form_link_delay_ms,"
           "abs_error_ns\n"
        << altitudeM / M_PER_KM << ',' << s.slantRangeM / M_PER_KM << ','
        << s.propagationDelay.GetSeconds() * MS_PER_S << ',' << simMs << ',' << closedFormMs << ','
        << errorNs << '\n';
    std::cout << std::fixed << std::setprecision(6) << "V1 overhead 600 km: propagation "
              << s.propagationDelay.GetSeconds() * MS_PER_S << " ms, link delay " << simMs
              << " ms (closed form " << closedFormMs << " ms)\n";
    return Verdict("V1 overhead delay within 1 ns of closed form", errorNs <= DELAY_TOLERANCE_NS);
}

/**
 * V2: altitude sweep (overhead delay grows with altitude) and one pass (range falls then
 * rises around the closest point).
 * @param dir output directory
 * @return true on PASS
 */
bool
RunV2(const std::string& dir)
{
    // (a) altitude sweep
    std::ofstream sweep(dir + "/v2-altitude-sweep.csv");
    sweep << std::fixed << std::setprecision(6)
          << "altitude_km,sim_propagation_delay_ms,closed_form_propagation_delay_ms\n";
    bool increasing = true;
    double previousMs = -1.0;
    for (double altitudeKm : {200.0, 600.0, 1200.0, 2000.0})
    {
        const NtnPairState s = OverheadState(altitudeKm * M_PER_KM);
        const double simMs = s.propagationDelay.GetSeconds() * MS_PER_S;
        sweep << altitudeKm << ',' << simMs << ','
              << altitudeKm * M_PER_KM / ntn::SPEED_OF_LIGHT_M_PER_S * MS_PER_S << '\n';
        increasing = increasing && simMs > previousMs;
        previousMs = simMs;
    }
    bool ok = Verdict("V2a overhead delay increases with altitude", increasing);

    // (b) one pass over a terminal on the ground track, sampled every 10 s
    NodeContainer sat;
    sat.Create(1);
    NodeContainer ground;
    ground.Create(1);
    NtnHelper ntn;
    ntn.SetOrbitAttribute("InitialPhase", DoubleValue(330.0));
    ntn.InstallSatellites(sat);
    ntn.InstallGroundTerminal(ground.Get(0), 0.0, 0.0);
    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    const uint32_t groundId = ground.Get(0)->GetId();
    const uint32_t satId = sat.Get(0)->GetId();

    std::ofstream pass(dir + "/v2-single-pass.csv");
    pass << std::fixed << std::setprecision(6) << "time_s,elevation_deg,range_km,delay_ms\n";
    std::vector<double> rangesKm;
    const Time step = Seconds(10);
    const Time duration = Seconds(1000);
    for (Time t = Time(0); t <= duration; t += step)
    {
        Simulator::Schedule(t, [&, groundId, satId]() {
            manager->Update();
            const NtnPairState s = manager->GetPairState(groundId, satId);
            pass << Simulator::Now().GetSeconds() << ',' << s.elevationDeg << ','
                 << s.slantRangeM / M_PER_KM << ',' << s.linkDelay.GetSeconds() * MS_PER_S << '\n';
            rangesKm.push_back(s.slantRangeM / M_PER_KM);
        });
    }
    Simulator::Run();
    Simulator::Destroy();

    const auto minIt = std::min_element(rangesKm.begin(), rangesKm.end());
    const bool interiorMin = minIt != rangesKm.begin() && minIt != rangesKm.end() - 1;
    const bool fallsThenRises = std::is_sorted(rangesKm.begin(), minIt + 1, std::greater<>()) &&
                                std::is_sorted(minIt, rangesKm.end());
    std::cout << "V2b closest approach " << *minIt << " km at sample " << (minIt - rangesKm.begin())
              << " of " << rangesKm.size() << "\n";
    ok = Verdict("V2b range decreases then increases during one pass",
                 interiorMin && fallsThenRises) &&
         ok;
    return ok;
}

/**
 * V3a: one overhead pass at a boundary configuration; compares visibility duration and the
 * largest visible range with closed form.
 * @param out CSV stream
 * @param minElevationDeg MinElevation [deg]
 * @param altitudeKm orbit altitude [km]
 * @return true if both quantities match closed form within the sampling tolerance
 */
bool
RunBoundaryPass(std::ofstream& out, double minElevationDeg, double altitudeKm)
{
    const double altitudeM = altitudeKm * M_PER_KM;
    // Start a little before the satellite rises at 0 deg elevation.
    const double horizonAngleDeg = ClosedFormCentralAngleDeg(altitudeM, 0.0);
    const double startMarginDeg = 2.0;
    const double initialPhaseDeg = ntn::FULL_CIRCLE_DEG - horizonAngleDeg - startMarginDeg;

    NodeContainer sat;
    sat.Create(1);
    NodeContainer ground;
    ground.Create(1);
    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeM));
    ntn.SetOrbitAttribute("InitialPhase", DoubleValue(initialPhaseDeg));
    ntn.SetVisibilityAttribute("MinElevation", DoubleValue(minElevationDeg));
    ntn.InstallSatellites(sat);
    ntn.InstallGroundTerminal(ground.Get(0), 0.0, 0.0);
    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    const Time updateInterval = manager->GetUpdateInterval();

    Time riseTime;
    Time setTime;
    double maxVisibleRangeM = 0.0;
    manager->TraceConnectWithoutContext(
        "VisibilityChange",
        MakeCallback(+[](Time* rise, Time* set, uint32_t, uint32_t, bool visible) {
            (visible ? *rise : *set) = Simulator::Now();
        }).Bind(&riseTime, &setTime));
    manager->TraceConnectWithoutContext(
        "SlantRangeUpdate",
        MakeCallback(+[](double* maxRange, uint32_t, uint32_t, double rangeM) {
            *maxRange = std::max(*maxRange, rangeM);
        }).Bind(&maxVisibleRangeM));

    const double omegaDegPerS = ntn::RadToDeg(
        sat.Get(0)->GetObject<CircularOrbitMobilityModel>()->GetAngularVelocityRadPerS());
    const double passDegrees = 2.0 * (horizonAngleDeg + startMarginDeg);
    ntn.Start();
    Simulator::Stop(Seconds(passDegrees / omegaDegPerS));
    Simulator::Run();
    Simulator::Destroy();

    const double simVisibleS = (setTime - riseTime).GetSeconds();
    const double closedVisibleS =
        2.0 * ClosedFormCentralAngleDeg(altitudeM, minElevationDeg) / omegaDegPerS;
    const double closedMaxRangeM = ClosedFormRangeM(altitudeM, minElevationDeg);
    // Visibility is only checked every update, so each edge may be off by one interval, and
    // the largest sampled range by the distance flown in one interval.
    const double speedMps = (ntn::EARTH_RADIUS_M + altitudeM) * ntn::DegToRad(omegaDegPerS);
    const bool durationOk =
        std::abs(simVisibleS - closedVisibleS) <= 2.0 * updateInterval.GetSeconds();
    const bool rangeOk =
        maxVisibleRangeM <= closedMaxRangeM + 1.0 &&
        closedMaxRangeM - maxVisibleRangeM <= speedMps * updateInterval.GetSeconds();

    out << minElevationDeg << ',' << altitudeKm << ',' << simVisibleS << ',' << closedVisibleS
        << ',' << maxVisibleRangeM / M_PER_KM << ',' << closedMaxRangeM / M_PER_KM << ','
        << (durationOk && rangeOk ? "PASS" : "FAIL") << '\n';
    return Verdict("V3a minElevation=" + std::to_string(static_cast<int>(minElevationDeg)) +
                       " altitude=" + std::to_string(static_cast<int>(altitudeKm)) + " km",
                   durationOk && rangeOk);
}

/**
 * V3: boundary passes and the multi-satellite "associate only to visible" check.
 * @param dir output directory
 * @return true on PASS
 */
bool
RunV3(const std::string& dir)
{
    std::ofstream boundary(dir + "/v3-boundary.csv");
    boundary << std::fixed << std::setprecision(6)
             << "min_elevation_deg,altitude_km,sim_visible_s,closed_form_visible_s,"
                "sim_max_visible_range_km,closed_form_max_range_km,result\n";
    bool ok = true;
    for (double minElevationDeg : {0.0, 45.0})
    {
        for (double altitudeKm : {200.0, 2000.0})
        {
            ok = RunBoundaryPass(boundary, minElevationDeg, altitudeKm) && ok;
        }
    }

    // SRS V3: association changes only ever go to a visible satellite.
    const uint32_t numGround = 20;
    const uint32_t numSats = 12;
    NodeContainer sats;
    sats.Create(numSats);
    NodeContainer grounds;
    grounds.Create(numGround);
    NtnHelper ntn;
    ntn.InstallSatellites(sats);
    for (uint32_t k = 0; k < numGround; ++k)
    {
        const auto [latDeg, lonDeg] =
            NtnHelper::GetGroundTrackPoint(k * ntn::FULL_CIRCLE_DEG / numGround,
                                           (static_cast<int>(k % 3) - 1) * 2.0,
                                           ntn::DEFAULT_INCLINATION_DEG,
                                           0.0);
        ntn.InstallGroundTerminal(grounds.Get(k), latDeg, lonDeg);
    }
    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    std::map<uint32_t, uint32_t> violations;
    manager->TraceConnectWithoutContext(
        "AssociationChange",
        MakeCallback(+[](NtnLinkManager* m,
                         std::map<uint32_t, uint32_t>* bad,
                         uint32_t groundId,
                         int32_t,
                         int32_t newSatelliteId) {
            if (newSatelliteId != ntn::NO_SATELLITE &&
                !m->GetPairState(groundId, static_cast<uint32_t>(newSatelliteId)).visible)
            {
                ++(*bad)[groundId];
            }
        }).Bind(PeekPointer(manager), &violations));
    ntn.Start();
    Simulator::Stop(Seconds(1200));
    Simulator::Run();

    std::ofstream assoc(dir + "/v3-association-visible.csv");
    assoc << "ground_id,association_changes,handovers,non_visible_associations\n";
    uint32_t totalChanges = 0;
    uint32_t totalViolations = 0;
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        const uint32_t id = (*it)->GetId();
        assoc << id << ',' << manager->GetAssociationChangeCount(id) << ','
              << manager->GetHandoverCount(id) << ',' << violations[id] << '\n';
        totalChanges += manager->GetAssociationChangeCount(id);
        totalViolations += violations[id];
    }
    Simulator::Destroy();
    std::cout << "V3b " << totalChanges << " association changes, " << totalViolations
              << " to a non-visible satellite\n";
    ok = Verdict("V3b association changes only to visible satellites",
                 totalChanges > 0 && totalViolations == 0) &&
         ok;
    return ok;
}

/// Results of one V4 policy run.
struct PolicyRunResult
{
    uint32_t handovers{0};          //!< Satellite-to-satellite switches, all terminals
    uint32_t associationChanges{0}; //!< All AssociationChange events
    uint64_t samples{0};            //!< (terminal, update) samples
    uint64_t coveredSamples{0};     //!< Samples with a primary association
    double elevationSumDeg{0.0};    //!< Sum of primary elevations over covered samples
    double delaySumMs{0.0};         //!< Sum of primary link delays over covered samples
};

/**
 * V4 helper: run the Walker scenario with one policy and collect handovers, mean elevation and
 * mean delay of the primary association.
 * @param policyTypeId policy TypeId name
 * @return the results
 */
PolicyRunResult
RunPolicyScenario(const std::string& policyTypeId)
{
    // Walker delta 53 deg: 120 satellites in 10 planes, F = 1 (offset 360 / 120 = 3 deg). It is
    // dense enough that terminals usually see two or more satellites; in a sparse constellation
    // (e.g. 24 satellites) there is rarely a choice and both policies make the same handovers.
    constexpr uint32_t NUM_SATS = 120;
    constexpr uint32_t NUM_PLANES = 10;
    constexpr uint32_t NUM_GROUND = 20;
    NodeContainer sats;
    sats.Create(NUM_SATS);
    NodeContainer grounds;
    grounds.Create(NUM_GROUND);
    NtnHelper ntn;
    ntn.SetConstellationAttribute("NumOrbitalPlanes", UintegerValue(NUM_PLANES));
    ntn.SetConstellationAttribute("PlanePhaseOffset", DoubleValue(ntn::FULL_CIRCLE_DEG / NUM_SATS));
    ntn.SetLinkAttribute("UpdateInterval", TimeValue(Seconds(1)));
    ntn.SetPolicy(policyTypeId);
    ntn.InstallSatellites(sats);
    for (uint32_t k = 0; k < NUM_GROUND; ++k)
    {
        // Terminals between 45 deg south and 45 deg north, spread around the globe.
        const double latitudeDeg = -45.0 + 90.0 * k / (NUM_GROUND - 1);
        const double longitudeDeg = -180.0 + ntn::FULL_CIRCLE_DEG * k / NUM_GROUND;
        ntn.InstallGroundTerminal(grounds.Get(k), latitudeDeg, longitudeDeg);
    }
    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    PolicyRunResult result;
    manager->TraceConnectWithoutContext(
        "UpdateCompleted",
        MakeCallback(+[](NtnLinkManager* m, PolicyRunResult* r, Time) {
            const NodeContainer nodes = m->GetGroundNodes();
            for (auto it = nodes.Begin(); it != nodes.End(); ++it)
            {
                ++r->samples;
                const int32_t sat = m->GetAssociatedSatellite((*it)->GetId());
                if (sat == ntn::NO_SATELLITE)
                {
                    continue;
                }
                const NtnPairState pair =
                    m->GetPairState((*it)->GetId(), static_cast<uint32_t>(sat));
                ++r->coveredSamples;
                r->elevationSumDeg += pair.elevationDeg;
                r->delaySumMs += pair.linkDelay.GetSeconds() * MS_PER_S;
            }
        }).Bind(PeekPointer(manager), &result));
    ntn.Start();
    Simulator::Stop(Seconds(3600));
    Simulator::Run();
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        result.handovers += manager->GetHandoverCount((*it)->GetId());
        result.associationChanges += manager->GetAssociationChangeCount((*it)->GetId());
    }
    Simulator::Destroy();
    return result;
}

/**
 * V4: handover counts of the baseline and the sticky policy on the same scenario.
 * @param dir output directory
 * @return true if passed
 */
bool
RunV4(const std::string& dir)
{
    std::ofstream out(dir + "/v4-policy-comparison.csv");
    out << "policy,handovers,association_changes,coverage_fraction,mean_elevation_deg,"
           "mean_delay_ms\n"
        << std::fixed << std::setprecision(6);
    std::map<std::string, PolicyRunResult> results;
    for (const std::string policy : {"ns3::HighestElevationPolicy", "ns3::StickyAssociationPolicy"})
    {
        const PolicyRunResult r = RunPolicyScenario(policy);
        results[policy] = r;
        const double covered = static_cast<double>(r.coveredSamples);
        out << policy << ',' << r.handovers << ',' << r.associationChanges << ','
            << covered / r.samples << ',' << r.elevationSumDeg / covered << ','
            << r.delaySumMs / covered << '\n';
        std::cout << "V4 " << policy << ": " << r.handovers << " handovers, mean elevation "
                  << std::fixed << std::setprecision(2) << r.elevationSumDeg / covered
                  << " deg, mean delay " << std::setprecision(3) << r.delaySumMs / covered
                  << " ms\n";
    }
    const PolicyRunResult& base = results["ns3::HighestElevationPolicy"];
    const PolicyRunResult& sticky = results["ns3::StickyAssociationPolicy"];
    bool ok = Verdict("V4a sticky policy makes fewer handovers", sticky.handovers < base.handovers);
    ok = Verdict("V4b baseline has the lower mean delay",
                 base.delaySumMs / base.coveredSamples <
                     sticky.delaySumMs / sticky.coveredSamples) &&
         ok;
    ok = Verdict("V4c both policies give the same coverage",
                 base.coveredSamples == sticky.coveredSamples) &&
         ok;
    return ok;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string validationCase = "all";
    std::string outputDir = ".";
    CommandLine cmd(__FILE__);
    cmd.AddValue("case", "Validation case: V1, V2, V3, V4 or all", validationCase);
    cmd.AddValue("outputDir", "Directory for the result CSV files (must exist)", outputDir);
    cmd.Parse(argc, argv);

    NtnHelper::WriteRunInfo(outputDir + "/validation-run-info.txt", argc, argv);
    bool ok = true;
    if (validationCase == "V1" || validationCase == "all")
    {
        ok = RunV1(outputDir) && ok;
    }
    if (validationCase == "V2" || validationCase == "all")
    {
        ok = RunV2(outputDir) && ok;
    }
    if (validationCase == "V3" || validationCase == "all")
    {
        ok = RunV3(outputDir) && ok;
    }
    if (validationCase == "V4" || validationCase == "all")
    {
        ok = RunV4(outputDir) && ok;
    }
    std::cout << (ok ? "ALL VALIDATION CASES PASSED" : "SOME VALIDATION CASES FAILED") << "\n";
    return ok ? 0 : 1;
}
