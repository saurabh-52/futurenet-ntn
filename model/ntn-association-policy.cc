/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-association-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnAssociationPolicy");

NS_OBJECT_ENSURE_REGISTERED(NtnAssociationPolicy);
NS_OBJECT_ENSURE_REGISTERED(HighestElevationPolicy);

TypeId
NtnAssociationPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnAssociationPolicy").SetParent<Object>().SetGroupName("FuturenetNtn");
    return tid;
}

NtnAssociationPolicy::~NtnAssociationPolicy() = default;

TypeId
HighestElevationPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::HighestElevationPolicy")
                            .SetParent<NtnAssociationPolicy>()
                            .SetGroupName("FuturenetNtn")
                            .AddConstructor<HighestElevationPolicy>();
    return tid;
}

HighestElevationPolicy::HighestElevationPolicy()
{
    NS_LOG_FUNCTION(this);
}

HighestElevationPolicy::~HighestElevationPolicy()
{
    NS_LOG_FUNCTION(this);
}

NtnAssociationDecision
HighestElevationPolicy::Select(const NtnAssociationState& state)
{
    NS_LOG_FUNCTION(this << state.groundId << state.visibleCandidates.size());
    NtnAssociationDecision decision;
    double bestElevationDeg = 0.0;
    for (const auto& candidate : state.visibleCandidates)
    {
        // On equal elevation the lower node ID wins, whatever order the candidates come in.
        if (decision.satelliteId == ntn::NO_SATELLITE ||
            candidate.elevationDeg > bestElevationDeg ||
            (candidate.elevationDeg == bestElevationDeg &&
             candidate.satelliteId < static_cast<uint32_t>(decision.satelliteId)))
        {
            decision.satelliteId = static_cast<int32_t>(candidate.satelliteId);
            bestElevationDeg = candidate.elevationDeg;
        }
    }
    return decision;
}

} // namespace ns3
