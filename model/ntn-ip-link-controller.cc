/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-ip-link-controller.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/node.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnIpLinkController");

NS_OBJECT_ENSURE_REGISTERED(NtnIpLinkController);

TypeId
NtnIpLinkController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NtnIpLinkController")
                            .SetParent<Object>()
                            .SetGroupName("FuturenetNtn")
                            .AddConstructor<NtnIpLinkController>();
    return tid;
}

NtnIpLinkController::NtnIpLinkController()
{
    NS_LOG_FUNCTION(this);
}

NtnIpLinkController::~NtnIpLinkController()
{
    NS_LOG_FUNCTION(this);
}

void
NtnIpLinkController::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_links.clear();
    m_gatewayLinks.clear();
    m_manager = nullptr;
    Object::DoDispose();
}

void
NtnIpLinkController::Connect(Ptr<NtnLinkManager> manager)
{
    NS_LOG_FUNCTION(this << manager);
    NS_ABORT_MSG_IF(!manager, "Link manager must not be null");
    NS_ABORT_MSG_IF(m_manager, "NtnIpLinkController is already connected to a link manager");
    m_manager = manager;
    manager->TraceConnectWithoutContext(
        "LinkDelayUpdate",
        MakeCallback(&NtnIpLinkController::HandleLinkDelayUpdate, this));
    manager->TraceConnectWithoutContext(
        "AssociationChange",
        MakeCallback(&NtnIpLinkController::HandleAssociationChange, this));
    manager->TraceConnectWithoutContext(
        "SecondaryAssociationChange",
        MakeCallback(&NtnIpLinkController::HandleSecondaryAssociationChange, this));
    manager->TraceConnectWithoutContext(
        "GatewayLinkDelayUpdate",
        MakeCallback(&NtnIpLinkController::HandleGatewayLinkDelayUpdate, this));
    manager->TraceConnectWithoutContext(
        "GatewayAssociationChange",
        MakeCallback(&NtnIpLinkController::HandleGatewayAssociationChange, this));
}

NtnIpLinkController::IpLink
NtnIpLinkController::MakeLink(const std::string& name,
                              Ptr<PointToPointNetDevice> groundDevice,
                              Ptr<PointToPointNetDevice> satelliteDevice)
{
    NS_ABORT_MSG_IF(!groundDevice || !satelliteDevice, "Null device for link " << name);
    IpLink link;
    link.channel = DynamicCast<PointToPointChannel>(groundDevice->GetChannel());
    NS_ABORT_MSG_IF(!link.channel || link.channel != satelliteDevice->GetChannel(),
                    "The two devices of link " << name << " must share one point-to-point channel");
    link.groundIpv4 = groundDevice->GetNode()->GetObject<Ipv4>();
    link.satelliteIpv4 = satelliteDevice->GetNode()->GetObject<Ipv4>();
    NS_ABORT_MSG_IF(!link.groundIpv4 || !link.satelliteIpv4,
                    "Install the Internet stack on both ends of link " << name << " first");

    const int32_t groundInterface = link.groundIpv4->GetInterfaceForDevice(groundDevice);
    const int32_t satelliteInterface = link.satelliteIpv4->GetInterfaceForDevice(satelliteDevice);
    NS_ABORT_MSG_IF(groundInterface < 0 || satelliteInterface < 0,
                    "Assign IPv4 addresses to link " << name << " before registering it");
    link.groundInterface = static_cast<uint32_t>(groundInterface);
    link.satelliteInterface = static_cast<uint32_t>(satelliteInterface);
    const Ipv4InterfaceAddress groundAddress = link.groundIpv4->GetAddress(link.groundInterface, 0);
    link.groundAddress = groundAddress.GetLocal();
    link.mask = groundAddress.GetMask();
    link.satelliteAddress = link.satelliteIpv4->GetAddress(link.satelliteInterface, 0).GetLocal();
    return link;
}

void
NtnIpLinkController::AddLink(uint32_t groundId,
                             uint32_t satelliteId,
                             Ptr<PointToPointNetDevice> groundDevice,
                             Ptr<PointToPointNetDevice> satelliteDevice)
{
    NS_LOG_FUNCTION(this << groundId << satelliteId);
    NS_ABORT_MSG_IF(m_links.count({groundId, satelliteId}) != 0,
                    "Link " << groundId << "/" << satelliteId << " is already registered");
    IpLink link = MakeLink("ground " + std::to_string(groundId) + " / satellite " +
                               std::to_string(satelliteId),
                           groundDevice,
                           satelliteDevice);
    // Address assignment brings interfaces up; a link may only carry traffic while associated.
    SetLinkUp(link, false);
    m_links[{groundId, satelliteId}] = link;
}

void
NtnIpLinkController::AddGatewayLink(uint32_t gatewayId,
                                    uint32_t satelliteId,
                                    Ptr<PointToPointNetDevice> gatewayDevice,
                                    Ptr<PointToPointNetDevice> satelliteDevice)
{
    NS_LOG_FUNCTION(this << gatewayId << satelliteId);
    NS_ABORT_MSG_IF(m_gatewayLinks.count({gatewayId, satelliteId}) != 0,
                    "Gateway link " << gatewayId << "/" << satelliteId << " is already registered");
    IpLink link = MakeLink("gateway " + std::to_string(gatewayId) + " / satellite " +
                               std::to_string(satelliteId),
                           gatewayDevice,
                           satelliteDevice);
    SetLinkUp(link, false);
    m_gatewayLinks[{gatewayId, satelliteId}] = link;
}

const NtnIpLinkController::IpLink&
NtnIpLinkController::GetGatewayLink(uint32_t gatewayId, uint32_t satelliteId) const
{
    auto it = m_gatewayLinks.find({gatewayId, satelliteId});
    NS_ABORT_MSG_IF(it == m_gatewayLinks.end(),
                    "No IP link registered for gateway " << gatewayId << " / satellite "
                                                         << satelliteId);
    return it->second;
}

bool
NtnIpLinkController::IsGatewayLinkUp(uint32_t gatewayId, uint32_t satelliteId) const
{
    const IpLink& link = GetGatewayLink(gatewayId, satelliteId);
    return link.groundIpv4->IsUp(link.groundInterface) &&
           link.satelliteIpv4->IsUp(link.satelliteInterface);
}

Time
NtnIpLinkController::GetGatewayChannelDelay(uint32_t gatewayId, uint32_t satelliteId) const
{
    TimeValue delay;
    GetGatewayLink(gatewayId, satelliteId).channel->GetAttribute("Delay", delay);
    return delay.Get();
}

const NtnIpLinkController::IpLink&
NtnIpLinkController::GetLink(uint32_t groundId, uint32_t satelliteId) const
{
    auto it = m_links.find({groundId, satelliteId});
    NS_ABORT_MSG_IF(it == m_links.end(),
                    "No IP link registered for ground " << groundId << " / satellite "
                                                        << satelliteId);
    return it->second;
}

void
NtnIpLinkController::SetLinkUp(const IpLink& link, bool up)
{
    SetInterfaceUp(link.groundIpv4, link.groundInterface, up);
    SetInterfaceUp(link.satelliteIpv4, link.satelliteInterface, up);
}

void
NtnIpLinkController::SetInterfaceUp(Ptr<Ipv4> ipv4, uint32_t interface, bool up)
{
    // Only act on a real change: bringing an interface up that is already up would make the
    // routing protocol add its subnet route a second time.
    if (ipv4->IsUp(interface) == up)
    {
        return;
    }
    if (up)
    {
        ipv4->SetUp(interface);
    }
    else
    {
        ipv4->SetDown(interface);
    }
}

bool
NtnIpLinkController::IsAssociated(uint32_t groundId, uint32_t satelliteId) const
{
    return m_manager->GetPairState(groundId, satelliteId).associated;
}

bool
NtnIpLinkController::IsLinkUp(uint32_t groundId, uint32_t satelliteId) const
{
    const IpLink& link = GetLink(groundId, satelliteId);
    return link.groundIpv4->IsUp(link.groundInterface) &&
           link.satelliteIpv4->IsUp(link.satelliteInterface);
}

Time
NtnIpLinkController::GetChannelDelay(uint32_t groundId, uint32_t satelliteId) const
{
    TimeValue delay;
    GetLink(groundId, satelliteId).channel->GetAttribute("Delay", delay);
    return delay.Get();
}

void
NtnIpLinkController::HandleLinkDelayUpdate(uint32_t groundId, uint32_t satelliteId, Time delay)
{
    // A packet's delay is fixed when it is sent, so packets already in flight keep the delay
    // that was valid at their transmission time.
    GetLink(groundId, satelliteId).channel->SetAttribute("Delay", TimeValue(delay));
}

void
NtnIpLinkController::HandleAssociationChange(uint32_t groundId,
                                             int32_t oldSatelliteId,
                                             int32_t newSatelliteId)
{
    NS_LOG_FUNCTION(this << groundId << oldSatelliteId << newSatelliteId);
    // The old primary stays up if it is still a secondary association. Otherwise taking the
    // interface down also makes ns-3 drop the routes that used it, including the old default
    // route.
    if (oldSatelliteId != ntn::NO_SATELLITE &&
        !IsAssociated(groundId, static_cast<uint32_t>(oldSatelliteId)))
    {
        SetLinkUp(GetLink(groundId, static_cast<uint32_t>(oldSatelliteId)), false);
    }
    if (newSatelliteId == ntn::NO_SATELLITE)
    {
        return; // no satellite in view: the ground terminal has no route until one appears
    }

    const IpLink& link = GetLink(groundId, static_cast<uint32_t>(newSatelliteId));
    SetLinkUp(link, true);
    SetDefaultRoute(link.groundIpv4, link.satelliteAddress, link.groundInterface);
}

void
NtnIpLinkController::SetDefaultRoute(Ptr<Ipv4> ipv4, Ipv4Address nextHop, uint32_t interface)
{
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> routing = routingHelper.GetStaticRouting(ipv4);
    NS_ABORT_MSG_IF(!routing,
                    "Node " << ipv4->GetObject<Node>()->GetId() << " has no Ipv4StaticRouting");
    // Remove any remaining default route before adding the new one.
    for (uint32_t i = routing->GetNRoutes(); i > 0; --i)
    {
        const Ipv4RoutingTableEntry route = routing->GetRoute(i - 1);
        if (route.IsDefault())
        {
            routing->RemoveRoute(i - 1);
        }
    }
    routing->SetDefaultRoute(nextHop, interface);
}

void
NtnIpLinkController::HandleSecondaryAssociationChange(uint32_t groundId,
                                                      uint32_t satelliteId,
                                                      bool associated)
{
    NS_LOG_FUNCTION(this << groundId << satelliteId << associated);
    // A satellite that left the secondary set may have become the primary; keep it up then.
    SetLinkUp(GetLink(groundId, satelliteId), associated || IsAssociated(groundId, satelliteId));
}

void
NtnIpLinkController::HandleGatewayLinkDelayUpdate(uint32_t gatewayId,
                                                  uint32_t satelliteId,
                                                  Time delay)
{
    GetGatewayLink(gatewayId, satelliteId).channel->SetAttribute("Delay", TimeValue(delay));
}

void
NtnIpLinkController::HandleGatewayAssociationChange(uint32_t satelliteId,
                                                    int32_t oldGatewayId,
                                                    int32_t newGatewayId)
{
    NS_LOG_FUNCTION(this << satelliteId << oldGatewayId << newGatewayId);
    if (oldGatewayId != ntn::NO_SATELLITE)
    {
        // Also removes the satellite's default route and the old gateway's return routes.
        SetLinkUp(GetGatewayLink(static_cast<uint32_t>(oldGatewayId), satelliteId), false);
    }
    if (newGatewayId == ntn::NO_SATELLITE)
    {
        return; // no gateway in view: traffic towards the ground network is dropped
    }
    const IpLink& link = GetGatewayLink(static_cast<uint32_t>(newGatewayId), satelliteId);
    SetLinkUp(link, true);
    SetDefaultRoute(link.satelliteIpv4, link.groundAddress, link.satelliteInterface);

    // Return path: the gateway reaches every ground terminal link subnet of this satellite
    // through it. A ground link that is currently down simply drops such packets at the
    // satellite, as a real network would without coverage.
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> gatewayRouting = routingHelper.GetStaticRouting(link.groundIpv4);
    NS_ABORT_MSG_IF(!gatewayRouting, "Gateway " << newGatewayId << " has no Ipv4StaticRouting");
    for (const auto& [key, groundLink] : m_links)
    {
        if (key.second == satelliteId)
        {
            gatewayRouting->AddNetworkRouteTo(groundLink.groundAddress.CombineMask(groundLink.mask),
                                              groundLink.mask,
                                              link.satelliteAddress,
                                              link.groundInterface);
        }
    }
}

} // namespace ns3
