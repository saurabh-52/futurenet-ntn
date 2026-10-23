/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn
 * Advanced example: several ground terminals, a ring of LEO satellites, real UDP traffic.
 *
 * - numSats satellites are spread evenly on one orbit plane; numGround terminals are spread
 *   along the satellites' ground track (alternately slightly left/right of it).
 * - Every ground terminal sends a UDP packet every packetInterval to the shared satellite
 *   service address; the packet travels over the point-to-point link of whichever satellite the
 *   terminal is associated with, so its delay follows the current geometry.
 * - FlowMonitor measures the IP flows; the console prints per-terminal association changes,
 *   handovers, packets and mean one-way delay.
 *
 * SRS demonstration size (20 ground terminals, 12 satellites):
 *   ./ns3 run "ntn-example-advanced --numGround=20 --numSats=12"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/futurenet-ntn-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

#include <iomanip>
#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NtnExampleAdvanced");

namespace
{

/// Per-ground-terminal totals collected from FlowMonitor.
struct GroundFlowTotals
{
    uint64_t txPackets{0}; //!< Packets sent by IP
    uint64_t rxPackets{0}; //!< Packets received by the satellites
    Time delaySum;         //!< Sum of one-way delays of received packets
    uint64_t rxBytes{0};   //!< Bytes received
};

/**
 * Map every IPv4 address of the ground terminals to the terminal's node ID, so FlowMonitor
 * flows (identified by source address) can be attributed to a terminal.
 * @param grounds ground terminal nodes
 * @return address -> node ID
 */
std::map<Ipv4Address, uint32_t>
MapAddressesToGround(const NodeContainer& grounds)
{
    std::map<Ipv4Address, uint32_t> owner;
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        Ptr<Ipv4> ipv4 = (*it)->GetObject<Ipv4>();
        for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
        {
            for (uint32_t a = 0; a < ipv4->GetNAddresses(i); ++a)
            {
                owner[ipv4->GetAddress(i, a).GetLocal()] = (*it)->GetId();
            }
        }
    }
    return owner;
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t numGround = 4;
    uint32_t numSats = 12;
    double altitudeKm = 600.0;
    double inclinationDeg = 53.0;
    double minElevationDeg = 10.0;
    double crossTrackDeg = 2.0;
    Time updateInterval = MilliSeconds(100);
    DataRate dataRate("100Mbps");
    Time processingDelay = MilliSeconds(1);
    Time simulationTime = Seconds(1200);
    Time packetInterval = Seconds(1);
    uint32_t packetSizeBytes = 512;
    Time csvSampleInterval = Seconds(1);
    std::string linkStateFile = "ntn-advanced-link-state.csv";
    std::string eventLogFile = "ntn-advanced-events.csv";
    std::string flowMonitorFile = "ntn-advanced-flowmon.xml";
    std::string runInfoFile = "ntn-advanced-run-info.txt";

    CommandLine cmd(__FILE__);
    cmd.Usage("Ground terminals + LEO ring with UDP traffic, handovers and FlowMonitor. "
              "RNG: --RngSeed / --RngRun.");
    cmd.AddValue("numGround", "Number of ground terminals", numGround);
    cmd.AddValue("numSats", "Number of satellites in the orbit plane (1 - 200)", numSats);
    cmd.AddValue("altitudeKm", "Orbit altitude [km] (200 - 2000)", altitudeKm);
    cmd.AddValue("inclinationDeg", "Orbit inclination [deg] (0 - 90)", inclinationDeg);
    cmd.AddValue("minElevationDeg",
                 "Minimum elevation for visibility [deg] (0 - 45)",
                 minElevationDeg);
    cmd.AddValue("crossTrackDeg",
                 "Sideways offset of every other terminal from the track [deg]",
                 crossTrackDeg);
    cmd.AddValue("updateInterval", "Link update interval (10ms - 10s)", updateInterval);
    cmd.AddValue("dataRate", "Logical link data rate (1Mbps - 10Gbps)", dataRate);
    cmd.AddValue("processingDelay", "Additive processing delay (0 - 100ms)", processingDelay);
    cmd.AddValue("simulationTime", "Simulated time", simulationTime);
    cmd.AddValue("packetInterval", "Time between UDP packets of one terminal", packetInterval);
    cmd.AddValue("packetSize", "UDP payload size [bytes]", packetSizeBytes);
    cmd.AddValue("csvSampleInterval",
                 "Time between link-state CSV samples (0 = every update)",
                 csvSampleInterval);
    cmd.AddValue("linkStateFile", "Link-state CSV output file ('' disables)", linkStateFile);
    cmd.AddValue("eventLogFile", "Common event log CSV output file ('' disables)", eventLogFile);
    cmd.AddValue("flowMonitorFile", "FlowMonitor XML output file ('' disables)", flowMonitorFile);
    cmd.AddValue("runInfoFile", "Run information output file", runInfoFile);
    cmd.Parse(argc, argv);

    NodeContainer satellites;
    satellites.Create(numSats);
    NodeContainer grounds;
    grounds.Create(numGround);

    // ---- Constellation, terminals and link model ----
    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeKm * 1e3));
    ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(inclinationDeg));
    ntn.SetVisibilityAttribute("MinElevation", DoubleValue(minElevationDeg));
    ntn.SetDelayAttribute("ProcessingDelay", TimeValue(processingDelay));
    ntn.SetLinkAttribute("UpdateInterval", TimeValue(updateInterval));
    ntn.SetLinkAttribute("DataRate", DataRateValue(dataRate));
    ntn.InstallSatellites(satellites);
    for (uint32_t k = 0; k < numGround; ++k)
    {
        // Alternate -offset, 0, +offset so terminals do not all see identical geometry.
        const double offsetDeg = (static_cast<int>(k % 3) - 1) * crossTrackDeg;
        const auto [latDeg, lonDeg] =
            NtnHelper::GetGroundTrackPoint(k * 360.0 / numGround, offsetDeg, inclinationDeg, 0.0);
        ntn.InstallGroundTerminal(grounds.Get(k), latDeg, lonDeg);
    }

    // ---- IP network: standard Internet stack + one p2p link per ground-satellite pair ----
    InternetStackHelper internet;
    internet.Install(satellites);
    internet.Install(grounds);
    Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
    ntn.InstallIpLinks(addresses);

    // ---- Traffic: every terminal -> shared satellite service address ----
    const uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(satellites);
    sinkApps.Start(Seconds(0));

    UdpClientHelper client(ntn.GetSatelliteServiceAddress(), port);
    client.SetAttribute("Interval", TimeValue(packetInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSizeBytes));
    client.SetAttribute("MaxPackets", UintegerValue(0)); // 0 = unlimited
    ApplicationContainer clientApps = client.Install(grounds);
    clientApps.Start(Seconds(1));
    clientApps.Stop(simulationTime);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    ntn.EnableTracing(linkStateFile, eventLogFile, csvSampleInterval);
    ntn.Start();
    NtnHelper::WriteRunInfo(runInfoFile, argc, argv);

    Simulator::Stop(simulationTime);
    Simulator::Run();

    // ---- Results ----
    flowMonitor->CheckForLostPackets();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    const auto addressOwner = MapAddressesToGround(grounds);
    std::map<uint32_t, GroundFlowTotals> totals;
    for (const auto& [flowId, stats] : flowMonitor->GetFlowStats())
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flowId);
        auto owner = addressOwner.find(tuple.sourceAddress);
        if (owner == addressOwner.end())
        {
            continue;
        }
        GroundFlowTotals& t = totals[owner->second];
        t.txPackets += stats.txPackets;
        t.rxPackets += stats.rxPackets;
        t.delaySum += stats.delaySum;
        t.rxBytes += stats.rxBytes;
    }

    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    uint32_t totalChanges = 0;
    uint32_t totalHandovers = 0;
    std::cout << "ground  assoc_changes  handovers  tx_pkts  rx_pkts  mean_delay_ms\n";
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        const uint32_t id = (*it)->GetId();
        const GroundFlowTotals& t = totals[id];
        const double meanDelayMs =
            t.rxPackets > 0 ? t.delaySum.GetSeconds() * 1e3 / t.rxPackets : 0.0;
        totalChanges += manager->GetAssociationChangeCount(id);
        totalHandovers += manager->GetHandoverCount(id);
        std::cout << std::setw(6) << id << std::setw(15) << manager->GetAssociationChangeCount(id)
                  << std::setw(11) << manager->GetHandoverCount(id) << std::setw(9) << t.txPackets
                  << std::setw(9) << t.rxPackets << std::fixed << std::setprecision(3)
                  << std::setw(15) << meanDelayMs << "\n";
    }
    std::cout << "Total association changes: " << totalChanges
              << ", total handovers: " << totalHandovers << "\n";

    if (!flowMonitorFile.empty())
    {
        flowMonitor->SerializeToXmlFile(flowMonitorFile, false, false);
    }
    std::cout << "Outputs: " << linkStateFile << ", " << eventLogFile << ", " << flowMonitorFile
              << ", " << runInfoFile << "\n";
    Simulator::Destroy();
    return 0;
}
