/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_LINK_MANAGER_H
#define NTN_LINK_MANAGER_H

#include "ntn-association-policy.h"
#include "ntn-common-types.h"
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

    /** @return all registered ground terminals, in registration order */
    NodeContainer GetGroundNodes() const;

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

    /** Run one Update() and schedule the next one. */
    void PeriodicUpdate();

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

    Ptr<NtnVisibilityModel> m_visibility;  //!< Visibility rule
    Ptr<NtnPropagationDelayModel> m_delay; //!< Delay calculation
    Ptr<NtnAssociationPolicy> m_policy;    //!< Association decision
    Time m_updateInterval;                 //!< Time between updates
    DataRate m_dataRate;                   //!< Logical link data rate
    uint32_t m_maxAssociations;            //!< MaxAssociationsPerGround
    EventId m_updateEvent;                 //!< Next scheduled periodic update

    TracedCallback<uint32_t, uint32_t, bool> m_visibilityChangeTrace;    //!< VisibilityChange
    TracedCallback<uint32_t, int32_t, int32_t> m_associationChangeTrace; //!< AssociationChange
    /// SecondaryAssociationChange
    TracedCallback<uint32_t, uint32_t, bool> m_secondaryAssociationChangeTrace;
    TracedCallback<uint32_t, uint32_t, double> m_slantRangeUpdateTrace; //!< SlantRangeUpdate
    TracedCallback<uint32_t, uint32_t, Time> m_linkDelayUpdateTrace;    //!< LinkDelayUpdate
    TracedCallback<Time> m_updateCompletedTrace;                        //!< UpdateCompleted
};

} // namespace ns3

#endif // NTN_LINK_MANAGER_H
