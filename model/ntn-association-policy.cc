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
NS_OBJECT_ENSURE_REGISTERED(StickyAssociationPolicy);

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

TypeId
StickyAssociationPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::StickyAssociationPolicy")
                            .SetParent<NtnAssociationPolicy>()
                            .SetGroupName("FuturenetNtn")
                            .AddConstructor<StickyAssociationPolicy>();
    return tid;
}

StickyAssociationPolicy::StickyAssociationPolicy()
{
    NS_LOG_FUNCTION(this);
}

StickyAssociationPolicy::~StickyAssociationPolicy()
{
    NS_LOG_FUNCTION(this);
}

NtnAssociationDecision
StickyAssociationPolicy::Select(const NtnAssociationState& state)
{
    NS_LOG_FUNCTION(this << state.groundId << state.visibleCandidates.size());
    NtnAssociationDecision decision;
    const std::vector<NtnCandidate> ranked = RankByElevation(state.visibleCandidates);
    if (ranked.empty())
    {
        return decision;
    }
    auto isVisible = [&ranked](int64_t satelliteId) {
        return std::any_of(ranked.begin(), ranked.end(), [satelliteId](const NtnCandidate& c) {
            return static_cast<int64_t>(c.satelliteId) == satelliteId;
        });
    };
    auto isCurrentSecondary = [&state](uint32_t satelliteId) {
        return std::find(state.currentSecondarySatelliteIds.begin(),
                         state.currentSecondarySatelliteIds.end(),
                         satelliteId) != state.currentSecondarySatelliteIds.end();
    };

    // 1. Primary. `ranked` is in elevation order, so the first current secondary found in it is
    //    the highest one.
    if (state.currentSatelliteId != ntn::NO_SATELLITE && isVisible(state.currentSatelliteId))
    {
        decision.satelliteId = state.currentSatelliteId;
    }
    else
    {
        auto promoted = std::find_if(ranked.begin(), ranked.end(), [&](const NtnCandidate& c) {
            return isCurrentSecondary(c.satelliteId);
        });
        decision.satelliteId = static_cast<int32_t>(
            promoted != ranked.end() ? promoted->satelliteId : ranked.front().satelliteId);
    }

    // 2. Secondary: still-visible current ones first, then the highest of the rest.
    const std::size_t freePlaces = state.maxAssociations > 0 ? state.maxAssociations - 1 : 0;
    for (const auto& candidate : ranked)
    {
        if (decision.secondarySatelliteIds.size() < freePlaces &&
            static_cast<int32_t>(candidate.satelliteId) != decision.satelliteId &&
            isCurrentSecondary(candidate.satelliteId))
        {
            decision.secondarySatelliteIds.push_back(candidate.satelliteId);
        }
    }
    for (const auto& candidate : ranked)
    {
        const bool chosen =
            static_cast<int32_t>(candidate.satelliteId) == decision.satelliteId ||
            std::find(decision.secondarySatelliteIds.begin(),
                      decision.secondarySatelliteIds.end(),
                      candidate.satelliteId) != decision.secondarySatelliteIds.end();
        if (decision.secondarySatelliteIds.size() < freePlaces && !chosen)
        {
            decision.secondarySatelliteIds.push_back(candidate.satelliteId);
        }
    }
    return decision;
}

} // namespace ns3
