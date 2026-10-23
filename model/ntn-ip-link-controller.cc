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
}

void
NtnIpLinkController::AddLink(uint32_t groundId,
                             uint32_t satelliteId,
                             Ptr<PointToPointNetDevice> groundDevice,
                             Ptr<PointToPointNetDevice> satelliteDevice)
{
    NS_LOG_FUNCTION(this << groundId << satelliteId);
    NS_ABORT_MSG_IF(!groundDevice || !satelliteDevice, "Null link device");
    NS_ABORT_MSG_IF(m_links.count({groundId, satelliteId}) != 0,
                    "Link " << groundId << "/" << satelliteId << " is already registered");

    IpLink link;
    link.channel = DynamicCast<PointToPointChannel>(groundDevice->GetChannel());
    NS_ABORT_MSG_IF(!link.channel || link.channel != satelliteDevice->GetChannel(),
                    "The two devices of link " << groundId << "/" << satelliteId
                                               << " must share one point-to-point channel");
    link.groundIpv4 = groundDevice->GetNode()->GetObject<Ipv4>();
    link.satelliteIpv4 = satelliteDevice->GetNode()->GetObject<Ipv4>();
    NS_ABORT_MSG_IF(!link.groundIpv4 || !link.satelliteIpv4,
                    "Install the Internet stack on ground terminals and satellites first");

    const int32_t groundInterface = link.groundIpv4->GetInterfaceForDevice(groundDevice);
    const int32_t satelliteInterface = link.satelliteIpv4->GetInterfaceForDevice(satelliteDevice);
    NS_ABORT_MSG_IF(groundInterface < 0 || satelliteInterface < 0,
                    "Assign IPv4 addresses to link " << groundId << "/" << satelliteId
                                                     << " before registering it");
    link.groundInterface = static_cast<uint32_t>(groundInterface);
    link.satelliteInterface = static_cast<uint32_t>(satelliteInterface);
    link.satelliteAddress = link.satelliteIpv4->GetAddress(link.satelliteInterface, 0).GetLocal();

    // Address assignment brings interfaces up; a link may only carry traffic while associated.
    SetLinkUp(link, false);
    m_links[{groundId, satelliteId}] = link;
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
    if (up)
    {
        link.groundIpv4->SetUp(link.groundInterface);
        link.satelliteIpv4->SetUp(link.satelliteInterface);
    }
    else
    {
        link.groundIpv4->SetDown(link.groundInterface);
        link.satelliteIpv4->SetDown(link.satelliteInterface);
    }
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
    if (oldSatelliteId != ntn::NO_SATELLITE)
    {
        // Taking the interface down also makes ns-3 drop the routes that used it, including the
        // old default route.
        SetLinkUp(GetLink(groundId, static_cast<uint32_t>(oldSatelliteId)), false);
    }
    if (newSatelliteId == ntn::NO_SATELLITE)
    {
        return; // no satellite in view: the ground terminal has no route until one appears
    }

    const IpLink& link = GetLink(groundId, static_cast<uint32_t>(newSatelliteId));
    SetLinkUp(link, true);

    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> routing = routingHelper.GetStaticRouting(link.groundIpv4);
    NS_ABORT_MSG_IF(!routing, "Ground terminal " << groundId << " has no Ipv4StaticRouting");
    // Remove any remaining default route before adding the one via the new satellite.
    for (uint32_t i = routing->GetNRoutes(); i > 0; --i)
    {
        const Ipv4RoutingTableEntry route = routing->GetRoute(i - 1);
        if (route.IsDefault())
        {
            routing->RemoveRoute(i - 1);
        }
    }
    routing->SetDefaultRoute(link.satelliteAddress, link.groundInterface);
}

} // namespace ns3
