/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_HELPER_H
#define NTN_HELPER_H

#include "ns3/attribute.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/ntn-association-policy.h"
#include "ns3/ntn-ip-link-controller.h"
#include "ns3/ntn-link-manager.h"
#include "ns3/ntn-trace-writer.h"
#include "ns3/ntn-walker-constellation.h"
#include "ns3/object-factory.h"

#include <map>
#include <string>
#include <utility>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Sets up a satellite/ground scenario with a few explicit calls.
 *
 * Typical use:
 * @code
 *   NtnHelper ntn;
 *   ntn.SetOrbitAttribute("OrbitAltitude", DoubleValue(600e3));
 *   ntn.SetVisibilityAttribute("MinElevation", DoubleValue(10.0));
 *   ntn.InstallSatellites(satellites);                 // mobility + registration
 *   ntn.InstallGroundTerminal(ground.Get(0), 0.0, 0.0); // latitude, longitude [deg]
 *   InternetStackHelper().Install(allNodes);           // standard ns-3 step
 *   Ipv4AddressHelper addresses("10.0.0.0", "255.255.255.252");
 *   ntn.InstallIpLinks(addresses);                     // one p2p link per pair
 *   ntn.EnableTracing("link-state.csv", "events.csv", Seconds(1));
 *   ntn.Start();
 * @endcode
 *
 * Every step is explicit; there is no "configure everything" call.
 */
class NtnHelper
{
  public:
    NtnHelper();

    /**
     * Set an Attribute of the CircularOrbitMobilityModel of satellites installed afterwards.
     * @param name attribute name (OrbitAltitude, OrbitInclination, InitialPhase, Raan)
     * @param value attribute value
     */
    void SetOrbitAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnWalkerConstellation used by InstallSatellites().
     * @param name attribute name (NumOrbitalPlanes, PlanePhaseOffset, RaanSpread)
     * @param value attribute value
     */
    void SetConstellationAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnGroundTerminalMobilityModel of terminals installed afterwards.
     * @param name attribute name (GroundHeight; Latitude/Longitude are set per terminal)
     * @param value attribute value
     */
    void SetGroundAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnLinkManager.
     * @param name attribute name (UpdateInterval, DataRate)
     * @param value attribute value
     */
    void SetLinkAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnVisibilityModel.
     * @param name attribute name (MinElevation)
     * @param value attribute value
     */
    void SetVisibilityAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnPropagationDelayModel.
     * @param name attribute name (PropagationSpeed, ProcessingDelay)
     * @param value attribute value
     */
    void SetDelayAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an Attribute of the NtnIslModel (ISLs are switched on with
     * SetLinkAttribute("EnableIsl", BooleanValue(true))).
     * @param name attribute name (MaxIslRange, IslMinClearance)
     * @param value attribute value
     */
    void SetIslAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Use another association policy instead of the default HighestElevationPolicy.
     * @param policy the policy object
     */
    void SetPolicy(Ptr<NtnAssociationPolicy> policy);

    /**
     * Use another association policy, given by its TypeId name.
     * @param typeId e.g. "ns3::HighestElevationPolicy"
     */
    void SetPolicy(const std::string& typeId);

    /**
     * Give each satellite a CircularOrbitMobilityModel and register it with the link manager.
     * The N satellites are laid out by the NtnWalkerConstellation: divided equally between
     * NumOrbitalPlanes planes, spread evenly around each plane, with the plane RAANs fanned out
     * and PlanePhaseOffset between planes (see NtnWalkerConstellation). With one plane (the
     * default) satellite k gets InitialPhase = (InitialPhase + k * 360 / N) mod 360.
     * @param satellites 1 - 200 satellite nodes (SRS NumSatellites); a multiple of
     *        NumOrbitalPlanes
     */
    void InstallSatellites(NodeContainer satellites);

    /**
     * @param satelliteNodeId node ID of a satellite installed by InstallSatellites()
     * @return its plane, slot and orbit angles
     */
    NtnOrbitSlot GetOrbitSlot(uint32_t satelliteNodeId) const;

    /**
     * Place a fixed ground terminal and register it with the link manager.
     * @param node ground terminal node
     * @param latitudeDeg latitude [deg]
     * @param longitudeDeg longitude [deg]
     */
    void InstallGroundTerminal(Ptr<Node> node, double latitudeDeg, double longitudeDeg);

    /**
     * Place a fixed gateway ground station and register it with the link manager (SRS
     * M01-FR-11). Requires SetLinkAttribute("EnableGateway", BooleanValue(true)) first. The
     * gateway uses the same mobility model and GroundHeight as ground terminals.
     * @param node gateway node
     * @param latitudeDeg latitude [deg]
     * @param longitudeDeg longitude [deg]
     */
    void InstallGateway(Ptr<Node> node, double latitudeDeg, double longitudeDeg);

    /**
     * Register ground terminals that already have a MobilityModel (e.g. moving terminals).
     * Positions must be Earth-centred Cartesian coordinates in metres.
     * @param nodes ground terminal nodes
     */
    void InstallGroundTerminals(NodeContainer nodes);

    /**
     * Create one point-to-point link per installed ground-satellite pair (and per
     * gateway-satellite pair if gateways are installed), with the link manager's DataRate,
     * assign one subnet per link from `addresses`, and hand the links to an
     * NtnIpLinkController. Each satellite also gets the shared satellite service address and
     * each gateway the shared gateway service address. The Internet stack must already be
     * installed on all of these nodes.
     * @param addresses address helper; its network/mask defines the per-link subnets
     */
    void InstallIpLinks(Ipv4AddressHelper& addresses);

    /**
     * @param address address that every satellite owns; traffic sent to it is received by
     *        whichever satellite the sender is associated with. Default 10.255.255.1.
     */
    void SetSatelliteServiceAddress(Ipv4Address address);

    /** @return the shared satellite service address */
    Ipv4Address GetSatelliteServiceAddress() const;

    /**
     * @param address address that every gateway owns; traffic a ground terminal sends to it
     *        travels ground -> primary satellite -> that satellite's gateway. Default
     *        10.255.254.1.
     */
    void SetGatewayServiceAddress(Ipv4Address address);

    /** @return the shared gateway service address */
    Ipv4Address GetGatewayServiceAddress() const;

    /**
     * Write the link-state CSV, the common event log and/or the ISL CSV. An empty path disables
     * that file.
     * @param linkStateCsv link-state CSV path
     * @param eventLogCsv common event log path
     * @param sampleInterval minimum time between link-state / ISL samples (0 = every update)
     * @param islCsv ISL CSV path (rows only when EnableIsl is true)
     */
    void EnableTracing(const std::string& linkStateCsv,
                       const std::string& eventLogCsv,
                       Time sampleInterval = Time(0),
                       const std::string& islCsv = "");

    /** Start the periodic link updates (first update at the current simulation time). */
    void Start();

    /** @return the link manager (for traces, queries and advanced configuration) */
    Ptr<NtnLinkManager> GetLinkManager() const;

    /** @return the IP link controller, or nullptr before InstallIpLinks() */
    Ptr<NtnIpLinkController> GetIpLinkController() const;

    /**
     * Latitude/longitude of a point near a satellite ground track, for building scenarios.
     * @param alongTrackDeg angle along the orbit from the ascending node [deg]
     * @param crossTrackDeg angular offset sideways from the track [deg]
     * @param inclinationDeg orbit inclination [deg]
     * @param raanDeg orbit RAAN [deg]
     * @return (latitude, longitude) [deg]
     */
    static std::pair<double, double> GetGroundTrackPoint(double alongTrackDeg,
                                                         double crossTrackDeg,
                                                         double inclinationDeg,
                                                         double raanDeg);

    /**
     * Write a run-information file for reproducibility (Common Guidelines section 8.3):
     * ns-3 version, module Git commit, RNG seed and run number, and the command line.
     * @param path output file path
     * @param argc argument count of main()
     * @param argv arguments of main()
     */
    static void WriteRunInfo(const std::string& path, int argc, char* argv[]);

  private:
    ObjectFactory m_orbitFactory;                  //!< Creates satellite mobility models
    ObjectFactory m_groundFactory;                 //!< Creates ground terminal mobility models
    Ptr<NtnWalkerConstellation> m_constellation;   //!< Satellite layout over orbital planes
    std::map<uint32_t, NtnOrbitSlot> m_orbitSlots; //!< Slot of every installed satellite
    Ptr<NtnLinkManager> m_linkManager;             //!< The mechanism
    Ptr<NtnIpLinkController> m_ipController;       //!< Network actuation (after InstallIpLinks)
    Ptr<NtnTraceWriter> m_traceWriter;             //!< File output (after EnableTracing)
    Ipv4Address m_gatewayServiceAddress;           //!< Address shared by all gateways
    Ipv4Address m_serviceAddress;                  //!< Address shared by all satellites
};

} // namespace ns3

#endif // NTN_HELPER_H
