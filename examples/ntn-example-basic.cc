/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn
 * Basic example: one ground station watches one LEO satellite fly over it.
 *
 * The satellite starts below the horizon (InitialPhase 330 deg, i.e. 30 deg before passing
 * overhead), rises, passes straight over the station and sets again. The link-state CSV shows
 * the slant range and delay falling and then rising; the console prints a short table and the
 * visibility/association events.
 *
 * Run:  ./ns3 run "ntn-example-basic --simulationTime=1000s"
 * Seed: add --RngSeed=<n> --RngRun=<n> (the model is deterministic; they are recorded anyway)
 */

#include "ns3/core-module.h"
#include "ns3/futurenet-ntn-module.h"
#include "ns3/network-module.h"

#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NtnExampleBasic");

namespace
{

/**
 * Print elevation, range and delay of the first ground-satellite pair, then reschedule.
 * @param manager the link manager
 * @param groundId ground terminal node ID
 * @param satelliteId satellite node ID
 * @param interval time between two lines
 */
void
PrintPairState(Ptr<NtnLinkManager> manager, uint32_t groundId, uint32_t satelliteId, Time interval)
{
    const NtnPairState pair = manager->GetPairState(groundId, satelliteId);
    std::cout << std::fixed << std::setprecision(1) << std::setw(8) << Simulator::Now().GetSeconds()
              << std::setw(10) << pair.elevationDeg << std::setw(11) << pair.slantRangeM / 1e3
              << std::setprecision(3) << std::setw(10) << pair.linkDelay.GetSeconds() * 1e3
              << std::setw(9) << (pair.visible ? "yes" : "no") << std::setw(12)
              << (pair.associated ? "yes" : "no") << "\n";
    Simulator::Schedule(interval, &PrintPairState, manager, groundId, satelliteId, interval);
}

/**
 * Console echo of AssociationChange (the CSV event log holds the full record).
 * @param groundId ground terminal node ID
 * @param oldSatelliteId previous satellite (-1 = none)
 * @param newSatelliteId new satellite (-1 = none)
 */
void
AssociationChanged(uint32_t groundId, int32_t oldSatelliteId, int32_t newSatelliteId)
{
    std::cout << "  [" << Simulator::Now().As(Time::S) << "] ground " << groundId << " association "
              << oldSatelliteId << " -> " << newSatelliteId << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t numGround = 1;
    uint32_t numSats = 1;
    double altitudeKm = 600.0;
    double inclinationDeg = 53.0;
    double initialPhaseDeg = 330.0;
    double minElevationDeg = 10.0;
    Time updateInterval = MilliSeconds(100);
    DataRate dataRate("100Mbps");
    Time processingDelay = MilliSeconds(1);
    Time simulationTime = Seconds(1000);
    Time printInterval = Seconds(50);
    Time csvSampleInterval = Seconds(1);
    std::string linkStateFile = "ntn-basic-link-state.csv";
    std::string eventLogFile = "ntn-basic-events.csv";
    std::string runInfoFile = "ntn-basic-run-info.txt";

    CommandLine cmd(__FILE__);
    cmd.Usage("One ground station + one LEO satellite pass. RNG: --RngSeed / --RngRun.");
    cmd.AddValue("numGround", "Number of ground terminals (spread along the track)", numGround);
    cmd.AddValue("numSats", "Number of satellites in the orbit plane (1 - 200)", numSats);
    cmd.AddValue("altitudeKm", "Orbit altitude [km] (200 - 2000)", altitudeKm);
    cmd.AddValue("inclinationDeg", "Orbit inclination [deg] (0 - 90)", inclinationDeg);
    cmd.AddValue("initialPhaseDeg", "Phase of the first satellite at t=0 [deg]", initialPhaseDeg);
    cmd.AddValue("minElevationDeg",
                 "Minimum elevation for visibility [deg] (0 - 45)",
                 minElevationDeg);
    cmd.AddValue("updateInterval", "Link update interval (10ms - 10s)", updateInterval);
    cmd.AddValue("dataRate", "Logical link data rate (1Mbps - 10Gbps)", dataRate);
    cmd.AddValue("processingDelay", "Additive processing delay (0 - 100ms)", processingDelay);
    cmd.AddValue("simulationTime", "Simulated time", simulationTime);
    cmd.AddValue("printInterval", "Time between console table lines", printInterval);
    cmd.AddValue("csvSampleInterval",
                 "Time between link-state CSV samples (0 = every update)",
                 csvSampleInterval);
    cmd.AddValue("linkStateFile", "Link-state CSV output file ('' disables)", linkStateFile);
    cmd.AddValue("eventLogFile", "Common event log CSV output file ('' disables)", eventLogFile);
    cmd.AddValue("runInfoFile", "Run information output file", runInfoFile);
    cmd.Parse(argc, argv);

    NodeContainer satellites;
    satellites.Create(numSats);
    NodeContainer grounds;
    grounds.Create(numGround);

    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeKm * 1e3));
    ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(inclinationDeg));
    ntn.SetOrbitAttribute("InitialPhase", DoubleValue(initialPhaseDeg));
    ntn.SetVisibilityAttribute("MinElevation", DoubleValue(minElevationDeg));
    ntn.SetDelayAttribute("ProcessingDelay", TimeValue(processingDelay));
    ntn.SetLinkAttribute("UpdateInterval", TimeValue(updateInterval));
    ntn.SetLinkAttribute("DataRate", DataRateValue(dataRate));
    ntn.InstallSatellites(satellites);

    // Ground terminals sit on the satellite ground track, spread evenly along it, so each one
    // sees the satellite pass straight overhead.
    for (uint32_t k = 0; k < numGround; ++k)
    {
        const auto [latDeg, lonDeg] =
            NtnHelper::GetGroundTrackPoint(k * 360.0 / numGround, 0.0, inclinationDeg, 0.0);
        ntn.InstallGroundTerminal(grounds.Get(k), latDeg, lonDeg);
    }

    ntn.EnableTracing(linkStateFile, eventLogFile, csvSampleInterval);
    ntn.GetLinkManager()->TraceConnectWithoutContext("AssociationChange",
                                                     MakeCallback(&AssociationChanged));
    ntn.Start();
    NtnHelper::WriteRunInfo(runInfoFile, argc, argv);

    Ptr<CircularOrbitMobilityModel> orbit =
        satellites.Get(0)->GetObject<CircularOrbitMobilityModel>();
    std::cout << "Orbit altitude " << altitudeKm << " km, period "
              << orbit->GetOrbitPeriod().As(Time::S) << " (" << orbit->GetOrbitPeriod().GetMinutes()
              << " min)\n"
              << "Ground node " << grounds.Get(0)->GetId() << " vs satellite node "
              << satellites.Get(0)->GetId() << ":\n"
              << "  time_s  elev_deg   range_km  delay_ms  visible  associated\n";

    // Print just after each update instead of at the same instant, so the table never shows
    // values from before the update.
    Simulator::Schedule(MicroSeconds(1),
                        &PrintPairState,
                        ntn.GetLinkManager(),
                        grounds.Get(0)->GetId(),
                        satellites.Get(0)->GetId(),
                        printInterval);

    Simulator::Stop(simulationTime);
    Simulator::Run();
    std::cout << "Association changes of ground node " << grounds.Get(0)->GetId() << ": "
              << ntn.GetLinkManager()->GetAssociationChangeCount(grounds.Get(0)->GetId())
              << "\nOutputs: " << linkStateFile << ", " << eventLogFile << ", " << runInfoFile
              << "\n";
    Simulator::Destroy();
    return 0;
}
