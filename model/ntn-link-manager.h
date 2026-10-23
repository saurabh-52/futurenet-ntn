/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_LINK_MANAGER_H
#define NTN_LINK_MANAGER_H

#include "ntn-association-policy.h"
#include "ntn-common-types.h"
#include "ntn-isl-model.h"
#include "ntn-propagation-delay-model.h"
#include "ntn-visibility-model.h"

#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/mobility-model.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"

#include <map>
#include <utility>
#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * The mechanism that keeps ground-satellite connectivity up to date.
 *
 * Every UpdateInterval, Update() does, for each ground terminal:
 * 1. compute elevation, slant range and link delay to every satellite;
 * 2. mark each satellite visible or not (NtnVisibilityModel) and fire VisibilityChange on a
 *    change, plus SlantRangeUpdate and LinkDelayUpdate for every visible satellite;
 * 3. hand the visible candidates to the NtnAssociationPolicy and apply its decision, firing
 *    AssociationChange when the primary satellite changes and SecondaryAssociationChange when a
 *    secondary satellite is added or removed (only possible with MaxAssociationsPerGround > 1).
 *
 * With EnableGateway and registered gateways, each update first does the same geometry for
 * every gateway-satellite pair and lets each satellite use the visible gateway that sees it
 * highest (GatewayVisibilityChange, GatewayLinkDelayUpdate, GatewayAssociationChange), so
 * GetEndToEndPath() gives ground -> satellite -> gateway (SRS M01-FR-11).
 *
 * With EnableIsl, each update also checks every satellite pair with the NtnIslModel (range and
 * Earth clearance) and fires IslStateChange / IslDelayUpdate (SRS M01-FR-12). The manager only
 * reports which ISLs exist and their delay; routing over them is left to other modules.
 *
 * The manager only computes and reports state. Turning network links up or down is done by
 * whoever listens to the traces (NtnIpLinkController in this module), so other modules can
 * reuse the manager with their own network setup.
 *
 * Cost: one update is O(ground terminals x satellites) geometry checks. This is needed because
 * any satellite may come into view of any terminal; for the SRS demonstration size (20 x 12 =
 * 240 pairs every 100 ms) it is negligible. See doc/futurenet-ntn.rst for possible
 * optimisations.
 */
class NtnLinkManager : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnLinkManager();
    ~NtnLinkManager() override;

    /**
     * Register a ground terminal. The node must already have a MobilityModel.
     * @param node the ground terminal node
     */
    void AddGroundNode(Ptr<Node> node);

    /**
     * Register a satellite. The node must already have a MobilityModel.
     * @param node the satellite node
     */
    void AddSatelliteNode(Ptr<Node> node);

    /**
     * Register a gateway ground station (SRS M01-FR-11). Requires EnableGateway = true and a
     * MobilityModel on the node. Every satellite associates with the visible gateway that sees
     * it at the highest elevation, using the same visibility and delay models as the ground
     * terminals.
     * @param node the gateway node
     */
    void AddGatewayNode(Ptr<Node> node);

    /** @return all registered ground terminals, in registration order */
    NodeContainer GetGroundNodes() const;

    /** @return all registered gateways, in registration order */
    NodeContainer GetGatewayNodes() const;

    /** @return all registered satellites, in registration order */
    NodeContainer GetSatelliteNodes() const;

    /** @param model visibility model to use (replaces the default one) */
    void SetVisibilityModel(Ptr<NtnVisibilityModel> model);

    /** @return the visibility model in use */
    Ptr<NtnVisibilityModel> GetVisibilityModel() const;

    /** @param model delay model to use (replaces the default one) */
    void SetDelayModel(Ptr<NtnPropagationDelayModel> model);

    /** @return the delay model in use */
    Ptr<NtnPropagationDelayModel> GetDelayModel() const;

    /** @param policy association policy to use (replaces the default HighestElevationPolicy) */
    void SetPolicy(Ptr<NtnAssociationPolicy> policy);

    /** @param model ISL feasibility model to use (replaces the default one) */
    void SetIslModel(Ptr<NtnIslModel> model);

    /** @return the ISL feasibility model in use */
    Ptr<NtnIslModel> GetIslModel() const;

    /** @return the association policy in use */
    Ptr<NtnAssociationPolicy> GetPolicy() const;

    /** @param interval time between updates, 10 ms - 10 s */
    void SetUpdateInterval(Time interval);

    /** @return the time between updates */
    Time GetUpdateInterval() const;

    /** @param rate logical satellite link data rate, 1 Mbit/s - 10 Gbit/s */
    void SetDataRate(DataRate rate);

    /** @return the logical satellite link data rate */
    DataRate GetDataRate() const;

    /** @param maxAssociations most satellites a ground terminal may use at once, 1 - 4 */
    void SetMaxAssociationsPerGround(uint32_t maxAssociations);

    /** @return the most satellites a ground terminal may use at once */
    uint32_t GetMaxAssociationsPerGround() const;

    /**
     * @param enable allow gateways (SRS EnableGateway); cannot be switched off once a gateway
     *        is registered
     */
    void SetEnableGateway(bool enable);

    /** @return true if gateways are enabled */
    bool GetEnableGateway() const;

    /** @param enable evaluate inter-satellite links every update (SRS M01-FR-12) */
    void SetEnableIsl(bool enable);

    /** @return true if inter-satellite links are evaluated */
    bool GetEnableIsl() const;

    /** Run Update() now and then every UpdateInterval until Stop() or the end of the run. */
    void Start();

    /** Cancel the periodic updates. */
    void Stop();

    /**
     * Re-evaluate visibility, ranges, delays and associations of all pairs at the current
     * simulation time. Called periodically after Start(); may also be called directly.
     */
    void Update();

    /**
     * @param groundNodeId ground terminal node ID
     * @return the primary associated satellite node ID, or -1 if none is visible
     */
    int32_t GetAssociatedSatellite(uint32_t groundNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @return all associated satellites: the primary first, then the secondary ones by node ID;
     *         empty if none is visible
     */
    std::vector<uint32_t> GetAssociatedSatellites(uint32_t groundNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @param satelliteNodeId satellite node ID
     * @return the common link-state view of the pair (see NtnLinkState)
     */
    NtnLinkState GetLinkState(uint32_t groundNodeId, uint32_t satelliteNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @param satelliteNodeId satellite node ID
     * @return the full geometric/association state of the pair from the last update
     */
    NtnPairState GetPairState(uint32_t groundNodeId, uint32_t satelliteNodeId) const;

    /** @return the state of every pair, ordered by (ground ID, satellite ID) */
    std::vector<NtnPairState> GetPairStates() const;

    /**
     * Visible satellites and their slant ranges (SRS M01-FR-04) from the last update.
     * @param groundNodeId ground terminal node ID
     * @return the visible candidates, ordered by satellite ID
     */
    std::vector<NtnCandidate> GetVisibleSatellites(uint32_t groundNodeId) const;

    /**
     * @param nodeId any registered ground terminal or satellite
     * @return position and velocity of the node now (common MobilityState contract)
     */
    NtnMobilityState GetMobilityState(uint32_t nodeId) const;

    /**
     * @param satelliteNodeId satellite node ID
     * @return the gateway the satellite is associated with, or -1 if none is visible
     */
    int32_t GetSatelliteGateway(uint32_t satelliteNodeId) const;

    /**
     * @param gatewayNodeId gateway node ID
     * @param satelliteNodeId satellite node ID
     * @return geometry and association of the pair from the last update (groundId holds the
     *         gateway ID; `associated` means the satellite uses this gateway)
     */
    NtnPairState GetGatewayPairState(uint32_t gatewayNodeId, uint32_t satelliteNodeId) const;

    /** @return the state of every gateway-satellite pair, ordered by (gateway ID, satellite ID) */
    std::vector<NtnPairState> GetGatewayPairStates() const;

    /**
     * @param satelliteNodeId satellite node ID
     * @return number of GatewayAssociationChange events of this satellite
     */
    uint32_t GetGatewayChangeCount(uint32_t satelliteNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @return the current path ground -> primary satellite -> gateway and its delay
     */
    NtnEndToEndPath GetEndToEndPath(uint32_t groundNodeId) const;

    /**
     * @param satelliteA a satellite node ID
     * @param satelliteB another satellite node ID (order does not matter)
     * @return the ISL state of the pair from the last update (aborts if ISLs were never
     *         evaluated for this pair)
     */
    NtnIslState GetIslState(uint32_t satelliteA, uint32_t satelliteB) const;

    /** @return every ISL candidate pair, ordered by (satelliteA, satelliteB) */
    std::vector<NtnIslState> GetIslStates() const;

    /**
     * @param satelliteNodeId satellite node ID
     * @return the satellites it currently has an ISL with, ordered by node ID
     */
    std::vector<uint32_t> GetIslNeighbours(uint32_t satelliteNodeId) const;

    /**
     * @param satelliteA a satellite node ID
     * @param satelliteB another satellite node ID
     * @return the common link-state view of the ISL (source = satelliteA, destination =
     *         satelliteB, available = ISL up, propagation delay NaN while down)
     */
    NtnLinkState GetIslLinkState(uint32_t satelliteA, uint32_t satelliteB) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @return number of AssociationChange events of this terminal (including gaining or
     *         losing coverage)
     */
    uint32_t GetAssociationChangeCount(uint32_t groundNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @return number of handovers, i.e. direct switches from one satellite to another
     */
    uint32_t GetHandoverCount(uint32_t groundNodeId) const;

    /**
     * TracedCallback signature for VisibilityChange.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param visible new visibility
     */
    typedef void (*VisibilityChangeTracedCallback)(uint32_t groundId,
                                                   uint32_t satelliteId,
                                                   bool visible);

    /**
     * TracedCallback signature for AssociationChange.
     * @param groundId ground terminal node ID
     * @param oldSatelliteId previous satellite node ID (-1 = none)
     * @param newSatelliteId new satellite node ID (-1 = none)
     */
    typedef void (*AssociationChangeTracedCallback)(uint32_t groundId,
                                                    int32_t oldSatelliteId,
                                                    int32_t newSatelliteId);

    /**
     * TracedCallback signature for SecondaryAssociationChange.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param associated true if the satellite became a secondary association, false if it
     *        stopped being one
     */
    typedef void (*SecondaryAssociationChangeTracedCallback)(uint32_t groundId,
                                                             uint32_t satelliteId,
                                                             bool associated);

    /**
     * TracedCallback signature for GatewayAssociationChange.
     * @param satelliteId satellite node ID
     * @param oldGatewayId previous gateway node ID (-1 = none)
     * @param newGatewayId new gateway node ID (-1 = none)
     */
    typedef void (*GatewayAssociationChangeTracedCallback)(uint32_t satelliteId,
                                                           int32_t oldGatewayId,
                                                           int32_t newGatewayId);

    /**
     * TracedCallback signature for IslStateChange.
     * @param satelliteA lower satellite node ID
     * @param satelliteB higher satellite node ID
     * @param up true if the ISL became possible, false if it was lost
     */
    typedef void (*IslStateChangeTracedCallback)(uint32_t satelliteA, uint32_t satelliteB, bool up);

    /**
     * TracedCallback signature for SlantRangeUpdate.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param rangeM slant range [m]
     */
    typedef void (*SlantRangeUpdateTracedCallback)(uint32_t groundId,
                                                   uint32_t satelliteId,
                                                   double rangeM);

    /**
     * TracedCallback signature for LinkDelayUpdate.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param delay one-way link delay (propagation + processing)
     */
    typedef void (*LinkDelayUpdateTracedCallback)(uint32_t groundId,
                                                  uint32_t satelliteId,
                                                  Time delay);

    /**
     * TracedCallback signature for UpdateCompleted.
     * @param now simulation time of the finished update
     */
    typedef void (*UpdateCompletedTracedCallback)(Time now);

  protected:
    void DoDispose() override;

  private:
    /// A registered node and its mobility model.
    struct NodeEntry
    {
        Ptr<Node> node;              //!< The node
        Ptr<MobilityModel> mobility; //!< Its mobility model
    };

    /// Per-ground-terminal association bookkeeping.
    struct GroundEntry
    {
        NodeEntry entry;                                //!< Node and mobility
        int32_t associatedSatellite{ntn::NO_SATELLITE}; //!< Current primary association
        std::vector<uint32_t> secondarySatellites;      //!< Secondary associations, sorted
        uint32_t associationChanges{0};                 //!< Number of AssociationChange events
        uint32_t handovers{0};                          //!< Satellite-to-satellite switches
    };

    /// Gateway bookkeeping of one satellite.
    struct SatelliteGatewayEntry
    {
        int32_t gateway{ntn::NO_SATELLITE}; //!< Current gateway node ID (-1 = none)
        uint32_t changes{0};                //!< Number of GatewayAssociationChange events
    };

    /** Run one Update() and schedule the next one. */
    void PeriodicUpdate();

    /**
     * Update visibility and delay of every gateway-satellite pair and let every satellite pick
     * the gateway that sees it highest.
     * @param satellitePositionsM satellite positions, in m_satellites order
     */
    void UpdateGateways(const std::vector<Vector>& satellitePositionsM);

    /**
     * Update distance, clearance, delay and state of every satellite pair.
     * @param satellitePositionsM satellite positions, in m_satellites order
     */
    void UpdateIsls(const std::vector<Vector>& satellitePositionsM);

    /**
     * Abort with a clear message if a policy decision breaks the rules of
     * NtnAssociationDecision (only visible satellites, no duplicates, at most
     * MaxAssociationsPerGround in total).
     * @param state the state the policy decided on
     * @param decision the policy decision
     */
    void CheckDecision(const NtnAssociationState& state,
                       const NtnAssociationDecision& decision) const;

    /**
     * Apply a policy decision for one ground terminal.
     * @param ground the ground terminal
     * @param state the state the policy decided on (used to validate the decision)
     * @param decision the policy decision
     */
    void ApplyDecision(GroundEntry& ground,
                       const NtnAssociationState& state,
                       const NtnAssociationDecision& decision);

    /**
     * @param groundNodeId ground terminal node ID
     * @return the registered ground terminal (aborts if unknown)
     */
    const GroundEntry& GetGround(uint32_t groundNodeId) const;

    /**
     * @param groundNodeId ground terminal node ID
     * @param satelliteNodeId satellite node ID
     * @return the stored pair state (aborts if the pair is unknown)
     */
    const NtnPairState& GetPair(uint32_t groundNodeId, uint32_t satelliteNodeId) const;

    /**
     * @param node node to check
     * @return its mobility model (aborts with a message if it has none)
     */
    static Ptr<MobilityModel> RequireMobility(Ptr<Node> node);

    std::vector<GroundEntry> m_grounds;  //!< Ground terminals, in registration order
    std::vector<NodeEntry> m_satellites; //!< Satellites, in registration order
    /// State of every (ground ID, satellite ID) pair; std::map keeps a fixed, sorted order.
    std::map<std::pair<uint32_t, uint32_t>, NtnPairState> m_pairs;
    std::vector<NodeEntry> m_gateways; //!< Gateways, in registration order
    /// State of every (gateway ID, satellite ID) pair.
    std::map<std::pair<uint32_t, uint32_t>, NtnPairState> m_gatewayPairs;
    /// Gateway association of every satellite, by satellite node ID.
    std::map<uint32_t, SatelliteGatewayEntry> m_satelliteGateways;
    /// ISL state of every satellite pair (lower ID, higher ID), filled when ISLs are enabled.
    std::map<std::pair<uint32_t, uint32_t>, NtnIslState> m_islPairs;

    Ptr<NtnVisibilityModel> m_visibility;  //!< Visibility rule
    Ptr<NtnPropagationDelayModel> m_delay; //!< Delay calculation
    Ptr<NtnAssociationPolicy> m_policy;    //!< Association decision
    Ptr<NtnIslModel> m_isl;                //!< ISL feasibility rule
    Time m_updateInterval;                 //!< Time between updates
    DataRate m_dataRate;                   //!< Logical link data rate
    uint32_t m_maxAssociations;            //!< MaxAssociationsPerGround
    bool m_enableGateway;                  //!< EnableGateway
    bool m_enableIsl;                      //!< EnableIsl
    EventId m_updateEvent;                 //!< Next scheduled periodic update

    TracedCallback<uint32_t, uint32_t, bool> m_visibilityChangeTrace;    //!< VisibilityChange
    TracedCallback<uint32_t, int32_t, int32_t> m_associationChangeTrace; //!< AssociationChange
    /// SecondaryAssociationChange
    TracedCallback<uint32_t, uint32_t, bool> m_secondaryAssociationChangeTrace;
    TracedCallback<uint32_t, uint32_t, double> m_slantRangeUpdateTrace;      //!< SlantRangeUpdate
    TracedCallback<uint32_t, uint32_t, Time> m_linkDelayUpdateTrace;         //!< LinkDelayUpdate
    TracedCallback<Time> m_updateCompletedTrace;                             //!< UpdateCompleted
    TracedCallback<uint32_t, uint32_t, bool> m_gatewayVisibilityChangeTrace; //!< Gateway vis.
    /// GatewayAssociationChange
    TracedCallback<uint32_t, int32_t, int32_t> m_gatewayAssociationChangeTrace;
    TracedCallback<uint32_t, uint32_t, Time> m_gatewayLinkDelayUpdateTrace; //!< Gateway delay
    TracedCallback<uint32_t, uint32_t, bool> m_islStateChangeTrace;         //!< IslStateChange
    TracedCallback<uint32_t, uint32_t, Time> m_islDelayUpdateTrace;         //!< IslDelayUpdate
};

} // namespace ns3

#endif // NTN_LINK_MANAGER_H
