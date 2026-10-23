/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-link-manager.h"

#include "ntn-attribute-checkers.h"
#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnLinkManager");

NS_OBJECT_ENSURE_REGISTERED(NtnLinkManager);

TypeId
NtnLinkManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnLinkManager")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnLinkManager>()
            .AddAttribute("UpdateInterval",
                          "Time between two re-evaluations of visibility, delay and association "
                          "(project range 10 ms - 10 s).",
                          TimeValue(MilliSeconds(ntn::DEFAULT_UPDATE_INTERVAL_MS)),
                          MakeTimeAccessor(&NtnLinkManager::SetUpdateInterval,
                                           &NtnLinkManager::GetUpdateInterval),
                          MakeTimeChecker(MilliSeconds(ntn::MIN_UPDATE_INTERVAL_MS),
                                          MilliSeconds(ntn::MAX_UPDATE_INTERVAL_MS)))
            .AddAttribute(
                "DataRate",
                "Logical satellite link data rate [bit/s] (project range "
                "1 Mbit/s - 10 Gbit/s).",
                DataRateValue(DataRate(ntn::DEFAULT_DATA_RATE_BPS)),
                MakeDataRateAccessor(&NtnLinkManager::SetDataRate, &NtnLinkManager::GetDataRate),
                MakeNtnDataRateChecker(DataRate(ntn::MIN_DATA_RATE_BPS),
                                       DataRate(ntn::MAX_DATA_RATE_BPS)))
            .AddAttribute("MaxAssociationsPerGround",
                          "Most satellites a ground terminal may be associated with at the same "
                          "time: one primary (carries the traffic) plus secondary ones whose "
                          "links are kept up (project range 1 - 4).",
                          UintegerValue(ntn::DEFAULT_ASSOCIATIONS_PER_GROUND),
                          MakeUintegerAccessor(&NtnLinkManager::SetMaxAssociationsPerGround,
                                               &NtnLinkManager::GetMaxAssociationsPerGround),
                          MakeUintegerChecker<uint32_t>(ntn::MIN_ASSOCIATIONS_PER_GROUND,
                                                        ntn::MAX_ASSOCIATIONS_PER_GROUND))
            .AddTraceSource("VisibilityChange",
                            "A satellite became visible or invisible from a ground terminal.",
                            MakeTraceSourceAccessor(&NtnLinkManager::m_visibilityChangeTrace),
                            "ns3::NtnLinkManager::VisibilityChangeTracedCallback")
            .AddTraceSource("AssociationChange",
                            "A ground terminal changed its associated satellite (-1 = none).",
                            MakeTraceSourceAccessor(&NtnLinkManager::m_associationChangeTrace),
                            "ns3::NtnLinkManager::AssociationChangeTracedCallback")
            .AddTraceSource(
                "SecondaryAssociationChange",
                "A satellite was added to (true) or removed from (false) the secondary "
                "associations of a ground terminal (MaxAssociationsPerGround > 1 only).",
                MakeTraceSourceAccessor(&NtnLinkManager::m_secondaryAssociationChangeTrace),
                "ns3::NtnLinkManager::SecondaryAssociationChangeTracedCallback")
            .AddTraceSource("SlantRangeUpdate",
                            "Slant range [m] of a visible satellite, reported every update.",
                            MakeTraceSourceAccessor(&NtnLinkManager::m_slantRangeUpdateTrace),
                            "ns3::NtnLinkManager::SlantRangeUpdateTracedCallback")
            .AddTraceSource("LinkDelayUpdate",
                            "One-way link delay to a visible satellite, reported every update.",
                            MakeTraceSourceAccessor(&NtnLinkManager::m_linkDelayUpdateTrace),
                            "ns3::NtnLinkManager::LinkDelayUpdateTracedCallback")
            .AddTraceSource("UpdateCompleted",
                            "An update of all ground-satellite pairs has finished.",
                            MakeTraceSourceAccessor(&NtnLinkManager::m_updateCompletedTrace),
                            "ns3::NtnLinkManager::UpdateCompletedTracedCallback");
    return tid;
}

NtnLinkManager::NtnLinkManager()
    : m_visibility(CreateObject<NtnVisibilityModel>()),
      m_delay(CreateObject<NtnPropagationDelayModel>()),
      m_policy(CreateObject<HighestElevationPolicy>()),
      m_updateInterval(MilliSeconds(ntn::DEFAULT_UPDATE_INTERVAL_MS)),
      m_dataRate(ntn::DEFAULT_DATA_RATE_BPS),
      m_maxAssociations(ntn::DEFAULT_ASSOCIATIONS_PER_GROUND)
{
    NS_LOG_FUNCTION(this);
}

NtnLinkManager::~NtnLinkManager()
{
    NS_LOG_FUNCTION(this);
}

void
NtnLinkManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_updateEvent.Cancel();
    m_grounds.clear();
    m_satellites.clear();
    m_pairs.clear();
    m_visibility = nullptr;
    m_delay = nullptr;
    m_policy = nullptr;
    Object::DoDispose();
}

Ptr<MobilityModel>
NtnLinkManager::RequireMobility(Ptr<Node> node)
{
    NS_ABORT_MSG_IF(!node, "Null node");
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(!mobility,
                    "Node " << node->GetId() << " has no MobilityModel; install one first "
                            << "(e.g. with NtnHelper)");
    return mobility;
}

void
NtnLinkManager::AddGroundNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    Ptr<MobilityModel> mobility = RequireMobility(node);
    const uint32_t groundId = node->GetId();
    NS_ABORT_MSG_IF(
        std::any_of(m_grounds.begin(),
                    m_grounds.end(),
                    [groundId](const GroundEntry& g) { return g.entry.node->GetId() == groundId; }),
        "Ground node " << groundId << " is already registered");
    GroundEntry ground;
    ground.entry = {node, mobility};
    m_grounds.push_back(ground);
    for (const auto& sat : m_satellites)
    {
        const uint32_t satelliteId = sat.node->GetId();
        NtnPairState pair;
        pair.groundId = groundId;
        pair.satelliteId = satelliteId;
        m_pairs[{groundId, satelliteId}] = pair;
    }
}

void
NtnLinkManager::AddSatelliteNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    Ptr<MobilityModel> mobility = RequireMobility(node);
    const uint32_t satelliteId = node->GetId();
    NS_ABORT_MSG_IF(
        std::any_of(m_satellites.begin(),
                    m_satellites.end(),
                    [satelliteId](const NodeEntry& s) { return s.node->GetId() == satelliteId; }),
        "Satellite node " << satelliteId << " is already registered");
    m_satellites.push_back({node, mobility});
    for (const auto& ground : m_grounds)
    {
        const uint32_t groundId = ground.entry.node->GetId();
        NtnPairState pair;
        pair.groundId = groundId;
        pair.satelliteId = satelliteId;
        m_pairs[{groundId, satelliteId}] = pair;
    }
}

NodeContainer
NtnLinkManager::GetGroundNodes() const
{
    NodeContainer nodes;
    for (const auto& ground : m_grounds)
    {
        nodes.Add(ground.entry.node);
    }
    return nodes;
}

NodeContainer
NtnLinkManager::GetSatelliteNodes() const
{
    NodeContainer nodes;
    for (const auto& sat : m_satellites)
    {
        nodes.Add(sat.node);
    }
    return nodes;
}

void
NtnLinkManager::SetVisibilityModel(Ptr<NtnVisibilityModel> model)
{
    NS_ABORT_MSG_IF(!model, "Visibility model must not be null");
    m_visibility = model;
}

Ptr<NtnVisibilityModel>
NtnLinkManager::GetVisibilityModel() const
{
    return m_visibility;
}

void
NtnLinkManager::SetDelayModel(Ptr<NtnPropagationDelayModel> model)
{
    NS_ABORT_MSG_IF(!model, "Delay model must not be null");
    m_delay = model;
}

Ptr<NtnPropagationDelayModel>
NtnLinkManager::GetDelayModel() const
{
    return m_delay;
}

void
NtnLinkManager::SetPolicy(Ptr<NtnAssociationPolicy> policy)
{
    NS_ABORT_MSG_IF(!policy, "Association policy must not be null");
    m_policy = policy;
}

Ptr<NtnAssociationPolicy>
NtnLinkManager::GetPolicy() const
{
    return m_policy;
}

void
NtnLinkManager::SetUpdateInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    NS_ABORT_MSG_IF(interval < MilliSeconds(ntn::MIN_UPDATE_INTERVAL_MS) ||
                        interval > MilliSeconds(ntn::MAX_UPDATE_INTERVAL_MS),
                    "UpdateInterval " << interval.As(Time::MS) << " is outside the project range "
                                      << ntn::MIN_UPDATE_INTERVAL_MS << " - "
                                      << ntn::MAX_UPDATE_INTERVAL_MS << " ms");
    m_updateInterval = interval;
}

Time
NtnLinkManager::GetUpdateInterval() const
{
    return m_updateInterval;
}

void
NtnLinkManager::SetDataRate(DataRate rate)
{
    NS_LOG_FUNCTION(this << rate);
    NS_ABORT_MSG_IF(rate < DataRate(ntn::MIN_DATA_RATE_BPS) ||
                        rate > DataRate(ntn::MAX_DATA_RATE_BPS),
                    "DataRate " << rate << " is outside the project range 1 Mbit/s - 10 Gbit/s");
    m_dataRate = rate;
}

DataRate
NtnLinkManager::GetDataRate() const
{
    return m_dataRate;
}

void
NtnLinkManager::SetMaxAssociationsPerGround(uint32_t maxAssociations)
{
    NS_LOG_FUNCTION(this << maxAssociations);
    NS_ABORT_MSG_IF(maxAssociations < ntn::MIN_ASSOCIATIONS_PER_GROUND ||
                        maxAssociations > ntn::MAX_ASSOCIATIONS_PER_GROUND,
                    "MaxAssociationsPerGround " << maxAssociations
                                                << " is outside the project range "
                                                << ntn::MIN_ASSOCIATIONS_PER_GROUND << " - "
                                                << ntn::MAX_ASSOCIATIONS_PER_GROUND);
    m_maxAssociations = maxAssociations;
}

uint32_t
NtnLinkManager::GetMaxAssociationsPerGround() const
{
    return m_maxAssociations;
}

void
NtnLinkManager::Start()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(m_updateEvent.IsPending(), "NtnLinkManager::Start() called twice");
    m_updateEvent = Simulator::ScheduleNow(&NtnLinkManager::PeriodicUpdate, this);
}

void
NtnLinkManager::Stop()
{
    NS_LOG_FUNCTION(this);
    m_updateEvent.Cancel();
}

void
NtnLinkManager::PeriodicUpdate()
{
    Update();
    m_updateEvent = Simulator::Schedule(m_updateInterval, &NtnLinkManager::PeriodicUpdate, this);
}

void
NtnLinkManager::Update()
{
    NS_LOG_FUNCTION(this);
    const Time now = Simulator::Now();

    // Satellite positions do not depend on the ground terminal, so compute them once.
    std::vector<Vector> satellitePositionsM;
    satellitePositionsM.reserve(m_satellites.size());
    for (const auto& sat : m_satellites)
    {
        satellitePositionsM.push_back(sat.mobility->GetPosition());
    }

    for (auto& ground : m_grounds)
    {
        const uint32_t groundId = ground.entry.node->GetId();
        const Vector groundPositionM = ground.entry.mobility->GetPosition();

        NtnAssociationState state;
        state.now = now;
        state.groundId = groundId;
        state.currentSatelliteId = ground.associatedSatellite;
        state.maxAssociations = m_maxAssociations;
        state.currentSecondarySatelliteIds = ground.secondarySatellites;

        for (std::size_t i = 0; i < m_satellites.size(); ++i)
        {
            const uint32_t satelliteId = m_satellites[i].node->GetId();
            NtnPairState& pair = m_pairs.at({groundId, satelliteId});

            pair.elevationDeg =
                NtnVisibilityModel::ComputeElevationDeg(groundPositionM, satellitePositionsM[i]);
            pair.slantRangeM =
                NtnVisibilityModel::ComputeSlantRangeM(groundPositionM, satellitePositionsM[i]);
            pair.propagationDelay = m_delay->GetPropagationDelay(pair.slantRangeM);
            pair.linkDelay = m_delay->GetLinkDelay(pair.slantRangeM);

            const bool visible = m_visibility->IsVisibleAtElevation(pair.elevationDeg);
            if (visible != pair.visible)
            {
                pair.visible = visible;
                NS_LOG_INFO("t=" << now.As(Time::S) << " ground " << groundId << " satellite "
                                 << satelliteId << (visible ? " visible" : " not visible")
                                 << " (elevation " << pair.elevationDeg << " deg)");
                m_visibilityChangeTrace(groundId, satelliteId, visible);
            }
            if (visible)
            {
                m_slantRangeUpdateTrace(groundId, satelliteId, pair.slantRangeM);
                m_linkDelayUpdateTrace(groundId, satelliteId, pair.linkDelay);
                state.visibleCandidates.push_back(
                    {satelliteId, pair.elevationDeg, pair.slantRangeM, pair.linkDelay});
            }
        }

        ApplyDecision(ground, state, m_policy->Select(state));
    }
    m_updateCompletedTrace(now);
}

void
NtnLinkManager::CheckDecision(const NtnAssociationState& state,
                              const NtnAssociationDecision& decision) const
{
    const std::string policyName = m_policy->GetInstanceTypeId().GetName();
    auto isVisible = [&state](int64_t satelliteId) {
        return std::any_of(state.visibleCandidates.begin(),
                           state.visibleCandidates.end(),
                           [satelliteId](const NtnCandidate& c) {
                               return static_cast<int64_t>(c.satelliteId) == satelliteId;
                           });
    };
    // A policy may only pick visible satellites (or none); this guarantees SRS V3.
    NS_ABORT_MSG_IF(decision.satelliteId != ntn::NO_SATELLITE && !isVisible(decision.satelliteId),
                    "Policy " << policyName << " chose satellite " << decision.satelliteId
                              << " which is not visible from ground " << state.groundId);
    if (decision.secondarySatelliteIds.empty())
    {
        return;
    }
    NS_ABORT_MSG_IF(decision.satelliteId == ntn::NO_SATELLITE,
                    "Policy " << policyName << " chose secondary satellites but no primary one "
                              << "for ground " << state.groundId);
    NS_ABORT_MSG_IF(decision.secondarySatelliteIds.size() >= m_maxAssociations,
                    "Policy " << policyName << " chose 1 + "
                              << decision.secondarySatelliteIds.size() << " satellites for ground "
                              << state.groundId << " but MaxAssociationsPerGround is "
                              << m_maxAssociations);
    std::vector<uint32_t> secondary = decision.secondarySatelliteIds;
    std::sort(secondary.begin(), secondary.end());
    NS_ABORT_MSG_IF(std::adjacent_find(secondary.begin(), secondary.end()) != secondary.end(),
                    "Policy " << policyName << " chose a secondary satellite twice for ground "
                              << state.groundId);
    for (uint32_t satelliteId : secondary)
    {
        NS_ABORT_MSG_IF(!isVisible(satelliteId),
                        "Policy " << policyName << " chose secondary satellite " << satelliteId
                                  << " which is not visible from ground " << state.groundId);
        NS_ABORT_MSG_IF(static_cast<int32_t>(satelliteId) == decision.satelliteId,
                        "Policy " << policyName << " chose satellite " << satelliteId
                                  << " as both primary and secondary for ground "
                                  << state.groundId);
    }
}

void
NtnLinkManager::ApplyDecision(GroundEntry& ground,
                              const NtnAssociationState& state,
                              const NtnAssociationDecision& decision)
{
    CheckDecision(state, decision);

    const int32_t oldSatellite = ground.associatedSatellite;
    const int32_t newSatellite = decision.satelliteId;
    const std::vector<uint32_t> oldSecondary = ground.secondarySatellites;
    std::vector<uint32_t> newSecondary = decision.secondarySatelliteIds;
    std::sort(newSecondary.begin(), newSecondary.end());
    if (newSatellite == oldSatellite && newSecondary == oldSecondary)
    {
        return;
    }
    const uint32_t groundId = state.groundId;

    // Store the complete new association set before firing any trace, so listeners (e.g. the IP
    // link controller) already see the final state of this update. A satellite can move between
    // primary and secondary in one update; it must then stay associated throughout.
    auto setAssociated = [this, groundId](int64_t satelliteId, bool associated) {
        if (satelliteId != ntn::NO_SATELLITE)
        {
            m_pairs.at({groundId, static_cast<uint32_t>(satelliteId)}).associated = associated;
        }
    };
    setAssociated(oldSatellite, false);
    for (uint32_t satelliteId : oldSecondary)
    {
        setAssociated(satelliteId, false);
    }
    setAssociated(newSatellite, true);
    for (uint32_t satelliteId : newSecondary)
    {
        setAssociated(satelliteId, true);
    }
    ground.associatedSatellite = newSatellite;
    ground.secondarySatellites = newSecondary;

    // Traces: removed secondary satellites, then the primary change, then added ones.
    for (uint32_t satelliteId : oldSecondary)
    {
        if (!std::binary_search(newSecondary.begin(), newSecondary.end(), satelliteId))
        {
            m_secondaryAssociationChangeTrace(groundId, satelliteId, false);
        }
    }
    if (newSatellite != oldSatellite)
    {
        ++ground.associationChanges;
        if (oldSatellite != ntn::NO_SATELLITE && newSatellite != ntn::NO_SATELLITE)
        {
            ++ground.handovers;
        }
        NS_LOG_INFO("t=" << state.now.As(Time::S) << " ground " << groundId << " association "
                         << oldSatellite << " -> " << newSatellite);
        m_associationChangeTrace(groundId, oldSatellite, newSatellite);
    }
    for (uint32_t satelliteId : newSecondary)
    {
        if (!std::binary_search(oldSecondary.begin(), oldSecondary.end(), satelliteId))
        {
            m_secondaryAssociationChangeTrace(groundId, satelliteId, true);
        }
    }
}

const NtnLinkManager::GroundEntry&
NtnLinkManager::GetGround(uint32_t groundNodeId) const
{
    auto it = std::find_if(m_grounds.begin(), m_grounds.end(), [groundNodeId](const auto& g) {
        return g.entry.node->GetId() == groundNodeId;
    });
    NS_ABORT_MSG_IF(it == m_grounds.end(), "Node " << groundNodeId << " is not a ground terminal");
    return *it;
}

const NtnPairState&
NtnLinkManager::GetPair(uint32_t groundNodeId, uint32_t satelliteNodeId) const
{
    auto it = m_pairs.find({groundNodeId, satelliteNodeId});
    NS_ABORT_MSG_IF(it == m_pairs.end(),
                    "Unknown ground/satellite pair " << groundNodeId << "/" << satelliteNodeId);
    return it->second;
}

int32_t
NtnLinkManager::GetAssociatedSatellite(uint32_t groundNodeId) const
{
    return GetGround(groundNodeId).associatedSatellite;
}

std::vector<uint32_t>
NtnLinkManager::GetAssociatedSatellites(uint32_t groundNodeId) const
{
    const GroundEntry& ground = GetGround(groundNodeId);
    std::vector<uint32_t> satellites;
    if (ground.associatedSatellite != ntn::NO_SATELLITE)
    {
        satellites.push_back(static_cast<uint32_t>(ground.associatedSatellite));
    }
    satellites.insert(satellites.end(),
                      ground.secondarySatellites.begin(),
                      ground.secondarySatellites.end());
    return satellites;
}

NtnLinkState
NtnLinkManager::GetLinkState(uint32_t groundNodeId, uint32_t satelliteNodeId) const
{
    const NtnPairState& pair = GetPair(groundNodeId, satelliteNodeId);
    constexpr double BITS_PER_MBIT = 1e6;
    constexpr double MS_PER_S = 1e3;
    NtnLinkState state;
    state.sourceNodeId = groundNodeId;
    state.destinationNodeId = satelliteNodeId;
    state.available = pair.associated;
    state.dataRateMbps = static_cast<double>(m_dataRate.GetBitRate()) / BITS_PER_MBIT;
    state.propagationDelayMs = pair.visible ? pair.propagationDelay.GetSeconds() * MS_PER_S
                                            : std::numeric_limits<double>::quiet_NaN();
    state.lossRatio = 0.0;
    return state;
}

NtnPairState
NtnLinkManager::GetPairState(uint32_t groundNodeId, uint32_t satelliteNodeId) const
{
    return GetPair(groundNodeId, satelliteNodeId);
}

std::vector<NtnPairState>
NtnLinkManager::GetPairStates() const
{
    std::vector<NtnPairState> states;
    states.reserve(m_pairs.size());
    for (const auto& [key, pair] : m_pairs)
    {
        states.push_back(pair);
    }
    return states;
}

std::vector<NtnCandidate>
NtnLinkManager::GetVisibleSatellites(uint32_t groundNodeId) const
{
    GetGround(groundNodeId); // aborts for an unknown ground terminal
    std::vector<NtnCandidate> candidates;
    for (auto it = m_pairs.lower_bound({groundNodeId, 0});
         it != m_pairs.end() && it->first.first == groundNodeId;
         ++it)
    {
        if (it->second.visible)
        {
            candidates.push_back({it->second.satelliteId,
                                  it->second.elevationDeg,
                                  it->second.slantRangeM,
                                  it->second.linkDelay});
        }
    }
    return candidates;
}

NtnMobilityState
NtnLinkManager::GetMobilityState(uint32_t nodeId) const
{
    Ptr<MobilityModel> mobility;
    for (const auto& ground : m_grounds)
    {
        if (ground.entry.node->GetId() == nodeId)
        {
            mobility = ground.entry.mobility;
        }
    }
    for (const auto& sat : m_satellites)
    {
        if (sat.node->GetId() == nodeId)
        {
            mobility = sat.mobility;
        }
    }
    NS_ABORT_MSG_IF(!mobility, "Node " << nodeId << " is not registered with the link manager");
    NtnMobilityState state;
    state.nodeId = nodeId;
    state.positionM = mobility->GetPosition();
    state.velocityMps = mobility->GetVelocity();
    state.timestamp = Simulator::Now();
    return state;
}

uint32_t
NtnLinkManager::GetAssociationChangeCount(uint32_t groundNodeId) const
{
    return GetGround(groundNodeId).associationChanges;
}

uint32_t
NtnLinkManager::GetHandoverCount(uint32_t groundNodeId) const
{
    return GetGround(groundNodeId).handovers;
}

} // namespace ns3
