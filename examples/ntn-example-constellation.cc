/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup futurenet-ntn
 * Constellation example (Phase 2): a multi-plane Walker constellation, gateway ground stations,
 * and ground terminals that reach the gateways through the satellites.
 *
 * - numSats satellites in numPlanes orbital planes (Walker delta i:N/P/F with F = walkerF);
 * - numGateways gateways on an east-west line through the region centre;
 * - numGround terminals on a grid around the same centre;
 * - every terminal runs a UDP echo client towards the shared gateway service address; the
 *   request travels terminal -> primary satellite -> that satellite's gateway, the echo comes
 *   back the same way. FlowMonitor measures both directions.
 *
 * Choose the association policy with --policy=highest|sticky and up to 4 simultaneous
 * associations per terminal with --maxAssociations.
 *
 *   ./ns3 run "ntn-example-constellation"
 *   ./ns3 run "ntn-example-constellation --policy=sticky --maxAssociations=2"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/futurenet-ntn-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NtnExampleConstellation");

namespace
{

/// Totals of one traffic direction collected from FlowMonitor.
struct DirectionTotals
{
    uint64_t txPackets{0}; //!< Packets sent
    uint64_t rxPackets{0}; //!< Packets received
    Time delaySum;         //!< Sum of one-way delays of received packets
};

/**
 * Collect every IPv4 address of a set of nodes.
 * @param nodes the nodes
 * @return their addresses
 */
std::set<Ipv4Address>
CollectAddresses(const NodeContainer& nodes)
{
    std::set<Ipv4Address> addresses;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Ptr<Ipv4> ipv4 = (*it)->GetObject<Ipv4>();
        for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
        {
            for (uint32_t a = 0; a < ipv4->GetNAddresses(i); ++a)
            {
                addresses.insert(ipv4->GetAddress(i, a).GetLocal());
            }
        }
    }
    return addresses;
}

/**
 * Print one direction's totals.
 * @param name direction name
 * @param t totals
 */
void
PrintDirection(const std::string& name, const DirectionTotals& t)
{
    constexpr double MS_PER_S = 1e3;
    const double meanDelayMs =
        t.rxPackets > 0 ? t.delaySum.GetSeconds() * MS_PER_S / t.rxPackets : 0.0;
    std::cout << name << ": " << t.txPackets << " sent, " << t.rxPackets << " received, mean "
              << "one-way delay " << std::fixed << std::setprecision(3) << meanDelayMs << " ms\n";
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
    double centreLatitudeDeg = 30.0;
    double centreLongitudeDeg = 0.0;
    double regionSizeDeg = 20.0;
    std::string policy = "highest";
    uint32_t maxAssociations = 1;
    Time updateInterval = MilliSeconds(100);
    DataRate dataRate("100Mbps");
    Time processingDelay = MilliSeconds(1);
    Time simulationTime = Seconds(1200);
    Time packetInterval = Seconds(1);
    uint32_t packetSizeBytes = 512;
    Time csvSampleInterval = Seconds(10);
    std::string linkStateFile = "ntn-constellation-link-state.csv";
    std::string eventLogFile = "ntn-constellation-events.csv";
    std::string flowMonitorFile = "ntn-constellation-flowmon.xml";
    std::string runInfoFile = "ntn-constellation-run-info.txt";

    CommandLine cmd(__FILE__);
    cmd.Usage("Multi-plane Walker constellation with gateways and UDP echo traffic "
              "terminal -> satellite -> gateway. RNG: --RngSeed / --RngRun.");
    cmd.AddValue("numGround", "Number of ground terminals", numGround);
    cmd.AddValue("numSats", "Number of satellites (1 - 200, a multiple of numPlanes)", numSats);
    cmd.AddValue("numPlanes", "Number of orbital planes (1 - 20)", numPlanes);
    cmd.AddValue("walkerF", "Walker phasing factor F: plane offset = F * 360 / numSats", walkerF);
    cmd.AddValue("numGateways", "Number of gateways (0 disables gateways)", numGateways);
    cmd.AddValue("altitudeKm", "Orbit altitude [km] (200 - 2000)", altitudeKm);
    cmd.AddValue("inclinationDeg", "Orbit inclination [deg] (0 - 90)", inclinationDeg);
    cmd.AddValue("minElevationDeg", "Minimum elevation [deg] (0 - 45)", minElevationDeg);
    cmd.AddValue("centreLatitudeDeg",
                 "Latitude of the service region centre [deg]",
                 centreLatitudeDeg);
    cmd.AddValue("centreLongitudeDeg",
                 "Longitude of the service region centre [deg]",
                 centreLongitudeDeg);
    cmd.AddValue("regionSizeDeg", "Width and height of the service region [deg]", regionSizeDeg);
    cmd.AddValue("policy", "Association policy: highest or sticky", policy);
    cmd.AddValue("maxAssociations", "Satellites per terminal at once (1 - 4)", maxAssociations);
    cmd.AddValue("updateInterval", "Link update interval (10ms - 10s)", updateInterval);
    cmd.AddValue("dataRate", "Logical link data rate (1Mbps - 10Gbps)", dataRate);
    cmd.AddValue("processingDelay",
                 "Additive processing delay per hop (0 - 100ms)",
                 processingDelay);
    cmd.AddValue("simulationTime", "Simulated time", simulationTime);
    cmd.AddValue("packetInterval", "Time between echo requests of one terminal", packetInterval);
    cmd.AddValue("packetSize", "UDP payload size [bytes]", packetSizeBytes);
    cmd.AddValue("csvSampleInterval",
                 "Time between link-state CSV samples (0 = every update)",
                 csvSampleInterval);
    cmd.AddValue("linkStateFile", "Link-state CSV output file ('' disables)", linkStateFile);
    cmd.AddValue("eventLogFile", "Common event log CSV output file ('' disables)", eventLogFile);
    cmd.AddValue("flowMonitorFile", "FlowMonitor XML output file ('' disables)", flowMonitorFile);
    cmd.AddValue("runInfoFile", "Run information output file", runInfoFile);
    cmd.Parse(argc, argv);

    const std::map<std::string, std::string> policies = {
        {"highest", "ns3::HighestElevationPolicy"},
        {"sticky", "ns3::StickyAssociationPolicy"}};
    NS_ABORT_MSG_IF(policies.count(policy) == 0,
                    "Unknown --policy=" << policy << "; use highest or sticky");
    NS_ABORT_MSG_IF(numGround == 0, "numGround must be at least 1");

    NodeContainer satellites;
    satellites.Create(numSats);
    NodeContainer grounds;
    grounds.Create(numGround);
    NodeContainer gateways;
    gateways.Create(numGateways);

    // ---- Constellation, gateways, terminals, link model ----
    NtnHelper ntn;
    ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(altitudeKm * 1e3));
    ntn.SetOrbitAttribute("OrbitInclination", DoubleValue(inclinationDeg));
    ntn.SetConstellationAttribute("NumOrbitalPlanes", UintegerValue(numPlanes));
    ntn.SetConstellationAttribute(
        "PlanePhaseOffset",
        DoubleValue(std::fmod(walkerF * ntn::FULL_CIRCLE_DEG / numSats, ntn::FULL_CIRCLE_DEG)));
    ntn.SetVisibilityAttribute("MinElevation", DoubleValue(minElevationDeg));
    ntn.SetDelayAttribute("ProcessingDelay", TimeValue(processingDelay));
    ntn.SetLinkAttribute("UpdateInterval", TimeValue(updateInterval));
    ntn.SetLinkAttribute("DataRate", DataRateValue(dataRate));
    ntn.SetLinkAttribute("MaxAssociationsPerGround", UintegerValue(maxAssociations));
    ntn.SetLinkAttribute("EnableGateway", BooleanValue(numGateways > 0));
    ntn.SetPolicy(policies.at(policy));
    ntn.InstallSatellites(satellites);

    // Gateways on an east-west line through the centre; terminals on a grid around it.
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

    // ---- IP network: Internet stack + one p2p link per ground/gateway-satellite pair ----
    InternetStackHelper internet;
    internet.Install(satellites);
    internet.Install(grounds);
    internet.Install(gateways);
    Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
    ntn.InstallIpLinks(addresses);

    // ---- Traffic: every terminal -> gateway service address -> echo back ----
    const uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(gateways);
    serverApps.Start(Seconds(0));
    UdpEchoClientHelper client(ntn.GetGatewayServiceAddress(), port);
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
    const std::set<Ipv4Address> groundAddresses = CollectAddresses(grounds);
    DirectionTotals uplink;
    DirectionTotals downlink;
    for (const auto& [flowId, stats] : flowMonitor->GetFlowStats())
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flowId);
        DirectionTotals& t = groundAddresses.count(tuple.sourceAddress) != 0 ? uplink : downlink;
        t.txPackets += stats.txPackets;
        t.rxPackets += stats.rxPackets;
        t.delaySum += stats.delaySum;
    }

    Ptr<NtnLinkManager> manager = ntn.GetLinkManager();
    uint32_t totalChanges = 0;
    uint32_t totalHandovers = 0;
    for (auto it = grounds.Begin(); it != grounds.End(); ++it)
    {
        totalChanges += manager->GetAssociationChangeCount((*it)->GetId());
        totalHandovers += manager->GetHandoverCount((*it)->GetId());
    }
    uint32_t gatewayChanges = 0;
    for (auto it = satellites.Begin(); it != satellites.End(); ++it)
    {
        gatewayChanges += manager->GetGatewayChangeCount((*it)->GetId());
    }

    std::cout << "Constellation: " << numSats << " satellites in " << numPlanes << " planes ("
              << numSats / numPlanes << " per plane), " << numGateways << " gateways, " << numGround
              << " terminals, policy " << policy << ", up to " << maxAssociations
              << " association(s) per terminal\n";
    std::cout << "Terminal association changes: " << totalChanges
              << ", handovers: " << totalHandovers
              << ", satellite gateway changes: " << gatewayChanges << "\n";
    PrintDirection("Uplink   (terminal -> gateway)", uplink);
    PrintDirection("Downlink (gateway -> terminal)", downlink);

    if (!flowMonitorFile.empty())
    {
        flowMonitor->SerializeToXmlFile(flowMonitorFile, false, false);
    }
    std::cout << "Outputs: " << linkStateFile << ", " << eventLogFile << ", " << flowMonitorFile
              << ", " << runInfoFile << "\n";
    Simulator::Destroy();
    return 0;
}
