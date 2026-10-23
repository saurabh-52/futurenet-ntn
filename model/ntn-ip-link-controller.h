/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_IP_LINK_CONTROLLER_H
#define NTN_IP_LINK_CONTROLLER_H

#include "ntn-link-manager.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/object.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-net-device.h"

#include <map>
#include <utility>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Applies NtnLinkManager decisions to real IPv4 point-to-point links.
 *
 * There is one point-to-point link per ground-satellite pair. The controller listens to the
 * link manager's traces and:
 * - LinkDelayUpdate: sets that link's channel Delay to the newly computed delay, so packets
 *   sent from now on experience the current geometry;
 * - AssociationChange: brings the IPv4 interfaces of the old link down and of the new link up
 *   (on both ends), and moves the ground terminal's default route to the new satellite;
 * - SecondaryAssociationChange: brings a secondary satellite's link up or down. Secondary links
 *   carry traffic addressed to that satellite's link address, but the default route always uses
 *   the primary satellite.
 *
 * All links start down; a link is only up while its ground terminal is associated (as primary
 * or secondary) with that satellite.
 */
class NtnIpLinkController : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnIpLinkController();
    ~NtnIpLinkController() override;

    /**
     * Start following a link manager's decisions.
     * @param manager the link manager to listen to
     */
    void Connect(Ptr<NtnLinkManager> manager);

    /**
     * Register the link of one ground-satellite pair. Both devices must already have an IPv4
     * address. The link is brought down until the pair becomes associated.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param groundDevice ground end of the link
     * @param satelliteDevice satellite end of the link
     */
    void AddLink(uint32_t groundId,
                 uint32_t satelliteId,
                 Ptr<PointToPointNetDevice> groundDevice,
                 Ptr<PointToPointNetDevice> satelliteDevice);

    /**
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @return true if the IPv4 interfaces of this pair's link are up
     */
    bool IsLinkUp(uint32_t groundId, uint32_t satelliteId) const;

    /**
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @return the current channel delay of this pair's link
     */
    Time GetChannelDelay(uint32_t groundId, uint32_t satelliteId) const;

  protected:
    void DoDispose() override;

  private:
    /// Everything needed to control one ground-satellite IP link.
    struct IpLink
    {
        Ptr<PointToPointChannel> channel; //!< Shared channel (holds the Delay)
        Ptr<Ipv4> groundIpv4;             //!< IPv4 stack of the ground terminal
        Ptr<Ipv4> satelliteIpv4;          //!< IPv4 stack of the satellite
        uint32_t groundInterface{0};      //!< Ground-side IPv4 interface index
        uint32_t satelliteInterface{0};   //!< Satellite-side IPv4 interface index
        Ipv4Address satelliteAddress;     //!< Satellite address on this link (next hop)
    };

    /**
     * LinkDelayUpdate handler.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param delay new one-way link delay
     */
    void HandleLinkDelayUpdate(uint32_t groundId, uint32_t satelliteId, Time delay);

    /**
     * AssociationChange handler.
     * @param groundId ground terminal node ID
     * @param oldSatelliteId previous satellite (-1 = none)
     * @param newSatelliteId new satellite (-1 = none)
     */
    void HandleAssociationChange(uint32_t groundId, int32_t oldSatelliteId, int32_t newSatelliteId);

    /**
     * SecondaryAssociationChange handler.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param associated true if the satellite became a secondary association
     */
    void HandleSecondaryAssociationChange(uint32_t groundId, uint32_t satelliteId, bool associated);

    /**
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @return true if the link manager currently has the pair associated (primary or secondary)
     */
    bool IsAssociated(uint32_t groundId, uint32_t satelliteId) const;

    /**
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @return the registered link (aborts if unknown)
     */
    const IpLink& GetLink(uint32_t groundId, uint32_t satelliteId) const;

    /**
     * Bring both ends of a link up or down.
     * @param link the link
     * @param up true for up, false for down
     */
    static void SetLinkUp(const IpLink& link, bool up);

    /**
     * Bring one IPv4 interface up or down; does nothing if it is already in that state.
     * @param ipv4 the IPv4 stack
     * @param interface interface index
     * @param up true for up, false for down
     */
    static void SetInterfaceUp(Ptr<Ipv4> ipv4, uint32_t interface, bool up);

    std::map<std::pair<uint32_t, uint32_t>, IpLink> m_links; //!< Links by (ground, satellite)
    Ptr<NtnLinkManager> m_manager;                           //!< Manager being followed
};

} // namespace ns3

#endif // NTN_IP_LINK_CONTROLLER_H
