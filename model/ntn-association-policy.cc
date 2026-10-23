/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-association-policy.h"

#include "ns3/log.h"

#include <algorithm>

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

std::vector<NtnCandidate>
NtnAssociationPolicy::RankByElevation(std::vector<NtnCandidate> candidates)
{
    std::sort(candidates.begin(),
              candidates.end(),
              [](const NtnCandidate& a, const NtnCandidate& b) {
                  if (a.elevationDeg != b.elevationDeg)
                  {
                      return a.elevationDeg > b.elevationDeg;
                  }
                  return a.satelliteId < b.satelliteId;
              });
    return candidates;
}

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
    const std::vector<NtnCandidate> ranked = RankByElevation(state.visibleCandidates);
    for (std::size_t i = 0; i < ranked.size() && i < state.maxAssociations; ++i)
    {
        if (i == 0)
        {
            decision.satelliteId = static_cast<int32_t>(ranked[i].satelliteId);
        }
        else
        {
            decision.secondarySatelliteIds.push_back(ranked[i].satelliteId);
        }
    }
    return decision;
}

} // namespace ns3
