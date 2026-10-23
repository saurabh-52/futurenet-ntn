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
    int32_t currentSatelliteId{ntn::NO_SATELLITE}; //!< Current association (-1 = none)
    std::vector<NtnCandidate> visibleCandidates;   //!< Visible satellites, sorted by node ID
};

/**
 * @ingroup futurenet-ntn
 * Result of a policy decision.
 */
struct NtnAssociationDecision
{
    /// Satellite node ID to associate with, or -1 (ntn::NO_SATELLITE) for no association.
    /// Must be one of the visible candidates; NtnLinkManager rejects anything else.
    int32_t satelliteId{ntn::NO_SATELLITE};
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
};

/**
 * @ingroup futurenet-ntn
 * Deterministic baseline policy (SRS M01-FR-06): pick the visible satellite with the highest
 * elevation. A high satellite is usually the closest one and the one that stays in view the
 * longest. Ties go to the lowest node ID so results never depend on container order.
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

} // namespace ns3

#endif // NTN_ASSOCIATION_POLICY_H
