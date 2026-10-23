/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_ASSOCIATION_POLICY_H
#define NTN_ASSOCIATION_POLICY_H

#include "ntn-constants.h"

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <cstdint>
#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * One satellite a ground terminal could associate with right now.
 */
struct NtnCandidate
{
    uint32_t satelliteId{0};  //!< Satellite node ID
    double elevationDeg{0.0}; //!< Elevation seen from the ground terminal [deg]
    double slantRangeM{0.0};  //!< Ground-satellite distance [m]
    Time linkDelay;           //!< One-way link delay (propagation + processing)
};

/**
 * @ingroup futurenet-ntn
 * Everything a policy may look at when choosing a satellite for one ground terminal.
 */
struct NtnAssociationState
{
    Time now;                                      //!< Simulation time of the decision
    uint32_t groundId{0};                          //!< Ground terminal node ID
    int32_t currentSatelliteId{ntn::NO_SATELLITE}; //!< Current primary association (-1 = none)
    std::vector<NtnCandidate> visibleCandidates;   //!< Visible satellites, sorted by node ID
    /// Most satellites the terminal may be associated with at once (MaxAssociationsPerGround):
    /// the primary plus up to maxAssociations - 1 secondary satellites.
    uint32_t maxAssociations{ntn::DEFAULT_ASSOCIATIONS_PER_GROUND};
    /// Current secondary associations, sorted by node ID (empty when maxAssociations = 1).
    std::vector<uint32_t> currentSecondarySatelliteIds;
};

/**
 * @ingroup futurenet-ntn
 * Result of a policy decision.
 */
struct NtnAssociationDecision
{
    /// Primary satellite node ID, or -1 (ntn::NO_SATELLITE) for no association. The primary
    /// satellite carries the terminal's traffic (default route). Must be one of the visible
    /// candidates; NtnLinkManager rejects anything else.
    int32_t satelliteId{ntn::NO_SATELLITE};
    /// Secondary satellites kept associated as well (links up, but no default route), in any
    /// order: at most maxAssociations - 1 distinct visible candidates, never the primary, and
    /// empty when there is no primary.
    std::vector<uint32_t> secondarySatelliteIds;
};

/**
 * @ingroup futurenet-ntn
 * Base class of association policies (the "decision" half of the mechanism/policy split).
 *
 * NtnLinkManager (the mechanism) computes geometry and delays, packs them into an
 * NtnAssociationState and asks the policy which satellite to use. A new policy, e.g. one
 * driven by optimisation or reinforcement learning, only needs to implement Select(); the
 * manager does not change.
 */
class NtnAssociationPolicy : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    ~NtnAssociationPolicy() override;

    /**
     * Choose the satellite for one ground terminal.
     * @param state current situation of the ground terminal
     * @return the decision
     */
    virtual NtnAssociationDecision Select(const NtnAssociationState& state) = 0;

  protected:
    /**
     * Order candidates from highest to lowest elevation; on equal elevation the lower node ID
     * comes first, so the result never depends on the input order.
     * @param candidates the candidates
     * @return the same candidates, ranked
     */
    static std::vector<NtnCandidate> RankByElevation(std::vector<NtnCandidate> candidates);
};

/**
 * @ingroup futurenet-ntn
 * Deterministic baseline policy (SRS M01-FR-06): pick the visible satellite with the highest
 * elevation. A high satellite is usually the closest one and the one that stays in view the
 * longest. Ties go to the lowest node ID so results never depend on container order.
 *
 * With MaxAssociationsPerGround = K > 1 the next K - 1 satellites in elevation order become the
 * secondary associations.
 */
class HighestElevationPolicy : public NtnAssociationPolicy
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    HighestElevationPolicy();
    ~HighestElevationPolicy() override;

    NtnAssociationDecision Select(const NtnAssociationState& state) override;
};

/**
 * @ingroup futurenet-ntn
 * "Sticky" policy: stay with the current satellite for as long as it is visible, and only
 * choose a new one when it drops below MinElevation.
 *
 * The baseline policy switches as soon as another satellite climbs higher, so a terminal between
 * two orbital planes can hand over back and forth. Each handover costs signalling and can lose
 * packets in flight, so this policy trades a lower average elevation (longer path, slightly more
 * delay) for fewer handovers.
 *
 * Rules (deterministic):
 * 1. Primary: keep the current primary if it is still visible. Otherwise promote the highest
 *    still-visible current secondary satellite (its link is already up, so this is a
 *    make-before-break handover). Otherwise take the highest visible satellite.
 * 2. Secondary (MaxAssociationsPerGround > 1): keep the current secondary satellites that are
 *    still visible, then fill the free places with the highest remaining satellites.
 * Ties go to the lowest node ID.
 */
class StickyAssociationPolicy : public NtnAssociationPolicy
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    StickyAssociationPolicy();
    ~StickyAssociationPolicy() override;

    NtnAssociationDecision Select(const NtnAssociationState& state) override;
};

} // namespace ns3

#endif // NTN_ASSOCIATION_POLICY_H
