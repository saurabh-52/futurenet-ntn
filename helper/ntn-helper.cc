/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-helper.h"

#include "ns3/abort.h"
#include "ns3/circular-orbit-mobility-model.h"
#include "ns3/double.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/ntn-constants.h"
#include "ns3/ntn-ground-terminal-mobility-model.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/rng-seed-manager.h"

#include <cmath>
#include <fstream>

// Filled in by CMake at configure time; the fallbacks keep the helper compilable on its own.
#ifndef FUTURENET_NTN_NS3_VERSION
#define FUTURENET_NTN_NS3_VERSION "unknown"
#endif
#ifndef FUTURENET_NTN_GIT_COMMIT
#define FUTURENET_NTN_GIT_COMMIT "unknown"
#endif

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnHelper");

namespace
{
/// Default address owned by every satellite (see NtnHelper::GetSatelliteServiceAddress()).
const char* const DEFAULT_SERVICE_ADDRESS = "10.255.255.1";
/// Default address owned by every gateway (see NtnHelper::GetGatewayServiceAddress()).
const char* const DEFAULT_GATEWAY_SERVICE_ADDRESS = "10.255.254.1";
} // namespace

NtnHelper::NtnHelper()
    : m_constellation(CreateObject<NtnWalkerConstellation>()),
      m_linkManager(CreateObject<NtnLinkManager>()),
      m_gatewayServiceAddress(DEFAULT_GATEWAY_SERVICE_ADDRESS),
      m_serviceAddress(DEFAULT_SERVICE_ADDRESS)
{
    m_orbitFactory.SetTypeId("ns3::CircularOrbitMobilityModel");
    m_groundFactory.SetTypeId("ns3::NtnGroundTerminalMobilityModel");
}

void
NtnHelper::SetOrbitAttribute(const std::string& name, const AttributeValue& value)
{
    m_orbitFactory.Set(name, value);
}

void
NtnHelper::SetConstellationAttribute(const std::string& name, const AttributeValue& value)
{
    m_constellation->SetAttribute(name, value);
}

void
NtnHelper::SetGroundAttribute(const std::string& name, const AttributeValue& value)
{
    m_groundFactory.Set(name, value);
}

void
NtnHelper::SetLinkAttribute(const std::string& name, const AttributeValue& value)
{
    m_linkManager->SetAttribute(name, value);
}

void
NtnHelper::SetVisibilityAttribute(const std::string& name, const AttributeValue& value)
{
    m_linkManager->GetVisibilityModel()->SetAttribute(name, value);
}

void
NtnHelper::SetDelayAttribute(const std::string& name, const AttributeValue& value)
{
    m_linkManager->GetDelayModel()->SetAttribute(name, value);
}

void
NtnHelper::SetPolicy(Ptr<NtnAssociationPolicy> policy)
{
    m_linkManager->SetPolicy(policy);
}

void
NtnHelper::SetPolicy(const std::string& typeId)
{
    ObjectFactory factory(typeId);
    Ptr<NtnAssociationPolicy> policy = factory.Create<NtnAssociationPolicy>();
    NS_ABORT_MSG_IF(!policy, typeId << " is not an NtnAssociationPolicy");
    m_linkManager->SetPolicy(policy);
}

void
NtnHelper::InstallSatellites(NodeContainer satellites)
{
    const uint32_t numSatellites = satellites.GetN();
    NS_ABORT_MSG_IF(
        numSatellites < ntn::MIN_NUM_SATELLITES || numSatellites > ntn::MAX_NUM_SATELLITES,
        "NumSatellites " << numSatellites << " is outside the project range "
                         << ntn::MIN_NUM_SATELLITES << " - " << ntn::MAX_NUM_SATELLITES);
    for (uint32_t k = 0; k < numSatellites; ++k)
    {
        Ptr<Node> node = satellites.Get(k);
        NS_ABORT_MSG_IF(node->GetObject<MobilityModel>(),
                        "Satellite node " << node->GetId() << " already has a MobilityModel");
        Ptr<CircularOrbitMobilityModel> orbit = m_orbitFactory.Create<CircularOrbitMobilityModel>();
        // The configured Raan and InitialPhase belong to the first satellite of the first plane;
        // the constellation derives every other satellite's angles from them.
        const NtnOrbitSlot slot =
            m_constellation->GetSlot(k, numSatellites, orbit->GetRaan(), orbit->GetInitialPhase());
        orbit->SetRaan(slot.raanDeg);
        orbit->SetInitialPhase(slot.initialPhaseDeg);
        node->AggregateObject(orbit);
        m_linkManager->AddSatelliteNode(node);
        m_orbitSlots[node->GetId()] = slot;
    }
}

NtnOrbitSlot
NtnHelper::GetOrbitSlot(uint32_t satelliteNodeId) const
{
    auto it = m_orbitSlots.find(satelliteNodeId);
    NS_ABORT_MSG_IF(it == m_orbitSlots.end(),
                    "Node " << satelliteNodeId << " was not installed by InstallSatellites()");
    return it->second;
}

void
NtnHelper::InstallGroundTerminal(Ptr<Node> node, double latitudeDeg, double longitudeDeg)
{
    NS_ABORT_MSG_IF(!node, "Null ground terminal node");
    NS_ABORT_MSG_IF(node->GetObject<MobilityModel>(),
                    "Ground node " << node->GetId() << " already has a MobilityModel; use "
                                   << "InstallGroundTerminals() for such nodes");
    Ptr<NtnGroundTerminalMobilityModel> mobility =
        m_groundFactory.Create<NtnGroundTerminalMobilityModel>();
    mobility->SetLatitude(latitudeDeg);
    mobility->SetLongitude(longitudeDeg);
    node->AggregateObject(mobility);
    m_linkManager->AddGroundNode(node);
}

void
NtnHelper::InstallGateway(Ptr<Node> node, double latitudeDeg, double longitudeDeg)
{
    NS_ABORT_MSG_IF(!node, "Null gateway node");
    NS_ABORT_MSG_IF(!m_linkManager->GetEnableGateway(),
                    "Gateways are disabled; call SetLinkAttribute(\"EnableGateway\", "
                    "BooleanValue(true)) before InstallGateway()");
    NS_ABORT_MSG_IF(node->GetObject<MobilityModel>(),
                    "Gateway node " << node->GetId() << " already has a MobilityModel");
    Ptr<NtnGroundTerminalMobilityModel> mobility =
        m_groundFactory.Create<NtnGroundTerminalMobilityModel>();
    mobility->SetLatitude(latitudeDeg);
    mobility->SetLongitude(longitudeDeg);
    node->AggregateObject(mobility);
    m_linkManager->AddGatewayNode(node);
}

void
NtnHelper::InstallGroundTerminals(NodeContainer nodes)
{
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        m_linkManager->AddGroundNode(*it);
    }
}

void
NtnHelper::InstallIpLinks(Ipv4AddressHelper& addresses)
{
    NS_ABORT_MSG_IF(m_ipController, "InstallIpLinks() may only be called once");
    m_ipController = CreateObject<NtnIpLinkController>();

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(m_linkManager->GetDataRate()));

    const NodeContainer grounds = m_linkManager->GetGroundNodes();
    const NodeContainer satellites = m_linkManager->GetSatelliteNodes();
    NS_ABORT_MSG_IF(grounds.GetN() == 0 || satellites.GetN() == 0,
                    "Install satellites and ground terminals before InstallIpLinks()");

    for (auto sat = satellites.Begin(); sat != satellites.End(); ++sat)
    {
        Ptr<Ipv4> ipv4 = (*sat)->GetObject<Ipv4>();
        NS_ABORT_MSG_IF(!ipv4, "Install the Internet stack before InstallIpLinks()");
        // Interface 0 is the loopback; a /32 there makes the satellite accept packets for the
        // shared service address arriving on any of its links.
        ipv4->AddAddress(0, Ipv4InterfaceAddress(m_serviceAddress, Ipv4Mask::GetOnes()));
    }

    for (auto ground = grounds.Begin(); ground != grounds.End(); ++ground)
    {
        for (auto sat = satellites.Begin(); sat != satellites.End(); ++sat)
        {
            NetDeviceContainer devices = p2p.Install(*ground, *sat);
            addresses.Assign(devices);
            addresses.NewNetwork();
            m_ipController->AddLink((*ground)->GetId(),
                                    (*sat)->GetId(),
                                    DynamicCast<PointToPointNetDevice>(devices.Get(0)),
                                    DynamicCast<PointToPointNetDevice>(devices.Get(1)));
        }
    }

    const NodeContainer gateways = m_linkManager->GetGatewayNodes();
    for (auto gateway = gateways.Begin(); gateway != gateways.End(); ++gateway)
    {
        Ptr<Ipv4> ipv4 = (*gateway)->GetObject<Ipv4>();
        NS_ABORT_MSG_IF(!ipv4,
                        "Install the Internet stack on the gateways before InstallIpLinks()");
        // Same trick as for the satellites: every gateway accepts the shared gateway address.
        ipv4->AddAddress(0, Ipv4InterfaceAddress(m_gatewayServiceAddress, Ipv4Mask::GetOnes()));
        for (auto sat = satellites.Begin(); sat != satellites.End(); ++sat)
        {
            NetDeviceContainer devices = p2p.Install(*gateway, *sat);
            addresses.Assign(devices);
            addresses.NewNetwork();
            m_ipController->AddGatewayLink((*gateway)->GetId(),
                                           (*sat)->GetId(),
                                           DynamicCast<PointToPointNetDevice>(devices.Get(0)),
                                           DynamicCast<PointToPointNetDevice>(devices.Get(1)));
        }
    }
    m_ipController->Connect(m_linkManager);
}

void
NtnHelper::SetSatelliteServiceAddress(Ipv4Address address)
{
    NS_ABORT_MSG_IF(m_ipController, "Set the service address before InstallIpLinks()");
    m_serviceAddress = address;
}

Ipv4Address
NtnHelper::GetSatelliteServiceAddress() const
{
    return m_serviceAddress;
}

void
NtnHelper::SetGatewayServiceAddress(Ipv4Address address)
{
    NS_ABORT_MSG_IF(m_ipController, "Set the gateway service address before InstallIpLinks()");
    m_gatewayServiceAddress = address;
}

Ipv4Address
NtnHelper::GetGatewayServiceAddress() const
{
    return m_gatewayServiceAddress;
}

void
NtnHelper::EnableTracing(const std::string& linkStateCsv,
                         const std::string& eventLogCsv,
                         Time sampleInterval)
{
    NS_ABORT_MSG_IF(m_traceWriter, "EnableTracing() may only be called once");
    m_traceWriter = CreateObject<NtnTraceWriter>();
    m_traceWriter->SetLinkStateSampleInterval(sampleInterval);
    if (!linkStateCsv.empty())
    {
        m_traceWriter->OpenLinkStateFile(linkStateCsv);
    }
    if (!eventLogCsv.empty())
    {
        m_traceWriter->OpenEventLogFile(eventLogCsv);
    }
    m_traceWriter->Connect(m_linkManager);
}

void
NtnHelper::Start()
{
    m_linkManager->Start();
}

Ptr<NtnLinkManager>
NtnHelper::GetLinkManager() const
{
    return m_linkManager;
}

Ptr<NtnIpLinkController>
NtnHelper::GetIpLinkController() const
{
    return m_ipController;
}

std::pair<double, double>
NtnHelper::GetGroundTrackPoint(double alongTrackDeg,
                               double crossTrackDeg,
                               double inclinationDeg,
                               double raanDeg)
{
    // Same rotations as CircularOrbitMobilityModel, applied to a unit vector: start in the
    // orbit plane (angle alongTrack from the ascending node), lean sideways by crossTrack
    // towards the orbit normal, then tilt by the inclination and turn by the RAAN.
    const double u = ntn::DegToRad(alongTrackDeg);
    const double c = ntn::DegToRad(crossTrackDeg);
    const double i = ntn::DegToRad(inclinationDeg);
    const double raan = ntn::DegToRad(raanDeg);
    const double xOrbit = std::cos(u) * std::cos(c);
    const double yOrbit = std::sin(u) * std::cos(c);
    const double zOrbit = std::sin(c);
    // Tilt about x by the inclination (the orbit normal is the plane's z axis).
    const double yTilt = yOrbit * std::cos(i) - zOrbit * std::sin(i);
    const double zTilt = yOrbit * std::sin(i) + zOrbit * std::cos(i);
    // Turn about the Earth axis by the RAAN.
    const double x = xOrbit * std::cos(raan) - yTilt * std::sin(raan);
    const double y = xOrbit * std::sin(raan) + yTilt * std::cos(raan);
    return {ntn::RadToDeg(std::asin(zTilt)), ntn::RadToDeg(std::atan2(y, x))};
}

void
NtnHelper::WriteRunInfo(const std::string& path, int argc, char* argv[])
{
    std::ofstream out(path);
    NS_ABORT_MSG_IF(!out.is_open(), "Cannot open run-info file " << path);
    out << "ns3_version=" << FUTURENET_NTN_NS3_VERSION << '\n'
        << "module=futurenet-ntn\n"
        << "git_commit=" << FUTURENET_NTN_GIT_COMMIT << '\n'
        << "rng_seed=" << RngSeedManager::GetSeed() << '\n'
        << "rng_run=" << RngSeedManager::GetRun() << '\n'
        << "command_line=";
    for (int i = 0; i < argc; ++i)
    {
        out << (i > 0 ? " " : "") << argv[i];
    }
    out << '\n';
}

} // namespace ns3
