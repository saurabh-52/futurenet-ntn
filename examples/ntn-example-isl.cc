/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn
 * Inter-satellite link example (Phase 3, SRS M01-FR-12).
 *
 * A Walker constellation with ISLs enabled, a few gateways and ground terminals around a region
 * centre. Every sampleInterval the example asks the link manager for the current state and
 * prints:
 * - how many ISLs are up and how many neighbours a satellite has on average;
 * - how many terminals have a satellite (coverage), a *direct* gateway path (their satellite
 *   sees a gateway, GetEndToEndPath()), and a path through ISLs to some satellite that sees a
 *   gateway;
 * - the mean end-to-end delay of the direct paths and of the shortest-delay paths via ISLs.
 *
 * The module itself only reports which ISLs exist and their delay; the shortest-path search here
 * (Dijkstra over IslDelay) is example analysis code that shows what a routing module (M02) could
 * do with that information. No packets are routed over ISLs.
 *
 *   ./ns3 run "ntn-example-isl"
 *   ./ns3 run "ntn-example-isl --maxIslRangeKm=3000 --numGateways=1"
 */

#include "ns3/core-module.h"
#include "ns3/futurenet-ntn-module.h"
#include "ns3/network-module.h"

#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NtnExampleIsl");

namespace
{

constexpr double MS_PER_S = 1e3; ///< Milliseconds per second

/// Totals accumulated over all samples.
struct IslTotals
{
    uint64_t terminalSamples{0}; //!< (terminal, sample) pairs
    uint64_t covered{0};         //!< ... with a primary satellite
    uint64_t direct{0};          //!< ... whose satellite sees a gateway
    uint64_t viaIsl{0};          //!< ... that reach a gateway directly or via ISLs
    double directDelaySumMs{0};  //!< Sum of direct end-to-end delays
    double islDelaySumMs{0};     //!< Sum of shortest delays (direct or via ISLs)
    uint32_t islUpEvents{0};     //!< IslStateChange(up) events
    uint32_t islDownEvents{0};   //!< IslStateChange(down) events
};

/**
 * Shortest one-way delay from one satellite to a gateway, over ISLs that are up (Dijkstra; edge
 * weight = ISL link delay, final hop = satellite-gateway link delay).
 * @param manager the link manager
 * @param source satellite node ID to start from
 * @return the delay [s], or +infinity if no gateway can be reached
 */
double
ShortestDelayToGatewayS(Ptr<NtnLinkManager> manager, uint32_t source)
{
    std::map<uint32_t, std::vector<std::pair<uint32_t, double>>> neighbours;
    for (const auto& isl : manager->GetIslStates())
    {
        if (isl.up)
        {
            neighbours[isl.satelliteA].emplace_back(isl.satelliteB, isl.linkDelay.GetSeconds());
            neighbours[isl.satelliteB].emplace_back(isl.satelliteA, isl.linkDelay.GetSeconds());
        }
    }
    std::map<uint32_t, double> best;
    using Entry = std::pair<double, uint32_t>; // (delay so far, satellite)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    queue.emplace(0.0, source);
    best[source] = 0.0;
    double bestTotalS = std::numeric_limits<double>::infinity();
    while (!queue.empty())
    {
        const auto [delayS, satellite] = queue.top();
        queue.pop();
        if (delayS > best[satellite] || delayS >= bestTotalS)
        {
            continue;
        }
        const int32_t gateway = manager->GetSatelliteGateway(satellite);
        if (gateway != ntn::NO_SATELLITE)
        {
            const double downS =
                manager->GetGatewayPairState(static_cast<uint32_t>(gateway), satellite)
                    .linkDelay.GetSeconds();
            bestTotalS = std::min(bestTotalS, delayS + downS);
        }
        for (const auto& [next, hopS] : neighbours[satellite])
        {
            const double candidateS = delayS + hopS;
            auto it = best.find(next);
            if (it == best.end() || candidateS < it->second)
            {
                best[next] = candidateS;
                queue.emplace(candidateS, next);
            }
        }
    }
    return bestTotalS;
}

/**
 * Take one sample of the ISL graph and of every terminal's paths, print one table row.
 * @param manager the link manager
 * @param totals totals to update
 */
void
Sample(Ptr<NtnLinkManager> manager, IslTotals* totals)
{
    uint32_t islsUp = 0;
    for (const auto& isl : manager->GetIslStates())
    {
        islsUp += isl.up ? 1 : 0;
    }
    const uint32_t numSatellites = manager->GetSatelliteNodes().GetN();
    const NodeContainer grounds = manager->GetGroundNodes();
    uint32_t covered = 0;
    uint32_t direct = 0;
    uint32_t viaIsl = 0;
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        const uint32_t groundId = (*it)->GetId();
        ++totals->terminalSamples;
        const int32_t satellite = manager->GetAssociatedSatellite(groundId);
        if (satellite == ntn::NO_SATELLITE)
        {
            continue;
        }
        ++covered;
        const NtnEndToEndPath path = manager->GetEndToEndPath(groundId);
        if (path.available)
        {
            ++direct;
            totals->directDelaySumMs += path.totalDelay.GetSeconds() * MS_PER_S;
        }
        const double toGatewayS =
            ShortestDelayToGatewayS(manager, static_cast<uint32_t>(satellite));
        if (std::isfinite(toGatewayS))
        {
            ++viaIsl;
            totals->islDelaySumMs += (path.groundLinkDelay.GetSeconds() + toGatewayS) * MS_PER_S;
        }
    }
    totals->covered += covered;
    totals->direct += direct;
    totals->viaIsl += viaIsl;
    std::cout << std::fixed << std::setw(8) << std::setprecision(0) << Simulator::Now().GetSeconds()
              << std::setw(9) << islsUp << std::setw(11) << std::setprecision(2)
              << 2.0 * islsUp / numSatellites << std::setw(9) << covered << std::setw(8) << direct
              << std::setw(9) << viaIsl << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t numGround = 20;
    uint32_t numSats = 120;
    uint32_t numPlanes = 10;
    uint32_t walkerF = 1;
    uint32_t numGateways = 3;
    double altitudeKm = 600.0;
    double inclinationDeg = 53.0;
    double minElevationDeg = 10.0;
    double maxIslRangeKm = 5000.0;
    double islMinClearanceKm = 0.0;
    double centreLatitudeDeg = 30.0;
    double centreLongitudeDeg = 0.0;
    double regionSizeDeg = 20.0;
    Time updateInterval = Seconds(1); // no traffic here, so a coarser update is enough
    Time simulationTime = Seconds(1200);
    Time sampleInterval = Seconds(60);
    std::string eventLogFile = "ntn-isl-events.csv";
    std::string islFile = "ntn-isl-links.csv";
    std::string runInfoFile = "ntn-isl-run-info.txt";

    CommandLine cmd(__FILE__);
    cmd.Usage("Walker constellation with inter-satellite links: ISL graph and gateway "
              "reachability over time. RNG: --RngSeed / --RngRun.");
    cmd.AddValue("numGround", "Number of ground terminals", numGround);
    cmd.AddValue("numSats", "Number of satellites (1 - 200, a multiple of numPlanes)", numSats);
    cmd.AddValue("numPlanes", "Number of orbital planes (1 - 20)", numPlanes);
    cmd.AddValue("walkerF", "Walker phasing factor F: plane offset = F * 360 / numSats", walkerF);
    cmd.AddValue("numGateways", "Number of gateways (0 disables gateways)", numGateways);
    cmd.AddValue("altitudeKm", "Orbit altitude [km] (200 - 2000)", altitudeKm);
    cmd.AddValue("inclinationDeg", "Orbit inclination [deg] (0 - 90)", inclinationDeg);
    cmd.AddValue("minElevationDeg", "Minimum elevation [deg] (0 - 45)", minElevationDeg);
    cmd.AddValue("maxIslRangeKm", "Longest usable ISL [km] (100 - 20000)", maxIslRangeKm);
    cmd.AddValue("islMinClearanceKm",
                 "Lowest altitude of an ISL line of sight [km] (0 - 1000)",
                 islMinClearanceKm);
    cmd.AddValue("centreLatitudeDeg",
                 "Latitude of the service region centre [deg]",
                 centreLatitudeDeg);
    cmd.AddValue("centreLongitudeDeg",
                 "Longitude of the service region centre [deg]",
                 centreLongitudeDeg);
    cmd.AddValue("regionSizeDeg", "Width and height of the service region [deg]", regionSizeDeg);
    cmd.AddValue("updateInterval", "Link update interval (10ms - 10s)", updateInterval);
    cmd.AddValue("simulationTime", "Simulated time", simulationTime);
    cmd.AddValue("sampleInterval", "Time between printed samples (also ISL CSV)", sampleInterval);
    cmd.AddValue("eventLogFile", "Common event log CSV output file ('' disables)", eventLogFile);
    cmd.AddValue("islFile", "ISL CSV output file ('' disables)", islFile);
    cmd.AddValue("runInfoFile", "Run information output file", runInfoFile);
    cmd.Parse(argc, argv);
    NS_ABORT_MSG_IF(numGround == 0, "numGround must be at least 1");
    NS_ABORT_MSG_IF(!sampleInterval.IsStrictlyPositive(), "sampleInterval must be positive");

    NodeContainer satellites;
    satellites.Create(numSats);
    NodeContainer grounds;
    grounds.Create(numGround);
    NodeContainer gateways;
    gateways.Create(numGateways);

    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeKm * 1e3));
    ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(inclinationDeg));
    ntn.SetConstellationAttribute("NumOrbitalPlanes", UintegerValue(numPlanes));
    ntn.SetConstellationAttribute(
        "PlanePhaseOffset",
        DoubleValue(std::fmod(walkerF * ntn::FULL_CIRCLE_DEG / numSats, ntn::FULL_CIRCLE_DEG)));
    ntn.SetVisibilityAttribute("MinElevation", DoubleValue(minElevationDeg));
    ntn.SetLinkAttribute("UpdateInterval", TimeValue(updateInterval));
    ntn.SetLinkAttribute("EnableGateway", BooleanValue(numGateways > 0));
    ntn.SetLinkAttribute("EnableIsl", BooleanValue(true));
    ntn.SetIslAttribute("MaxIslRange", DoubleValue(maxIslRangeKm * 1e3));
    ntn.SetIslAttribute("IslMinClearance", DoubleValue(islMinClearanceKm * 1e3));
    ntn.InstallSatellites(satellites);

    // Same placement as ntn-example-constellation: gateways on an east-west line through the
    // centre, terminals on a grid around it.
    for (uint32_t k = 0; k < numGateways; ++k)
    {
        const double offsetDeg = (k + 0.5) * regionSizeDeg / numGateways - regionSizeDeg / 2;
        ntn.InstallGateway(gateways.Get(k), centreLatitudeDeg, centreLongitudeDeg + offsetDeg);
    }
    const auto columns = static_cast<uint32_t>(std::ceil(std::sqrt(numGround)));
    const uint32_t rows = (numGround + columns - 1) / columns;
    for (uint32_t k = 0; k < numGround; ++k)
    {
        const double latDeg =
            centreLatitudeDeg + ((k / columns) + 0.5) * regionSizeDeg / rows - regionSizeDeg / 2;
        const double lonDeg = centreLongitudeDeg + ((k % columns) + 0.5) * regionSizeDeg / columns -
                              regionSizeDeg / 2;
        ntn.InstallGroundTerminal(grounds.Get(k), latDeg, lonDeg);
    }

    ntn.EnableTracing("", eventLogFile, sampleInterval, islFile);
    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    IslTotals totals;
    manager->TraceConnectWithoutContext(
        "IslStateChange",
        MakeCallback(+[](IslTotals* t, uint32_t, uint32_t, bool up) {
            ++(up ? t->islUpEvents : t->islDownEvents);
        }).Bind(&totals));
    ntn.Start();
    NtnHelper::WriteRunInfo(runInfoFile, argc, argv);

    // Sample just after the link update that happens at the same instant.
    std::cout << "  time_s  isls_up  neighbours  covered  direct  via_isl\n";
    const Time afterUpdate = MicroSeconds(1);
    for (Time t = Time(0); t < simulationTime; t += sampleInterval)
    {
        Simulator::Schedule(t + afterUpdate, &Sample, manager, &totals);
    }
    Simulator::Stop(simulationTime);
    Simulator::Run();

    const double samples = static_cast<double>(totals.terminalSamples);
    std::cout << std::setprecision(1) << "Terminal samples: covered "
              << 100.0 * totals.covered / samples << " %, direct gateway path "
              << 100.0 * totals.direct / samples << " %, gateway reachable with ISLs "
              << 100.0 * totals.viaIsl / samples << " %\n"
              << std::setprecision(3) << "Mean end-to-end delay: direct "
              << (totals.direct > 0 ? totals.directDelaySumMs / totals.direct : 0.0)
              << " ms, best with ISLs "
              << (totals.viaIsl > 0 ? totals.islDelaySumMs / totals.viaIsl : 0.0) << " ms\n"
              << "ISL events: " << totals.islUpEvents << " up, " << totals.islDownEvents
              << " down\n"
              << "Outputs: " << eventLogFile << ", " << islFile << ", " << runInfoFile << "\n";
    Simulator::Destroy();
    return 0;
}
