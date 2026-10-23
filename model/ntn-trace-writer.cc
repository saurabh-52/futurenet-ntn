/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-trace-writer.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <iomanip>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnTraceWriter");

NS_OBJECT_ENSURE_REGISTERED(NtnTraceWriter);

namespace
{
constexpr double M_PER_KM = 1e3;                 ///< Metres per kilometre
constexpr int TIME_DECIMALS = 6;                 ///< Decimals for time_s (microsecond resolution)
constexpr int VALUE_DECIMALS = 6;                ///< Decimals for ranges, delays and angles
const char* const MODULE_NAME = "futurenet-ntn"; ///< Value of the event-log `module` column
} // namespace

TypeId
NtnTraceWriter::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnTraceWriter")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnTraceWriter>()
            .AddAttribute("LinkStateSampleInterval",
                          "Minimum time between two link-state CSV samples; 0 writes every "
                          "update. Events are always logged.",
                          TimeValue(Time(0)),
                          MakeTimeAccessor(&NtnTraceWriter::SetLinkStateSampleInterval,
                                           &NtnTraceWriter::GetLinkStateSampleInterval),
                          MakeTimeChecker(Time(0)));
    return tid;
}

NtnTraceWriter::NtnTraceWriter()
    : m_sampleInterval(Time(0)),
      m_nextSampleTime(Time(0))
{
    NS_LOG_FUNCTION(this);
}

NtnTraceWriter::~NtnTraceWriter()
{
    NS_LOG_FUNCTION(this);
}

void
NtnTraceWriter::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_linkStateFile.is_open())
    {
        m_linkStateFile.close();
    }
    if (m_eventLogFile.is_open())
    {
        m_eventLogFile.close();
    }
    m_manager = nullptr;
    Object::DoDispose();
}

void
NtnTraceWriter::SetLinkStateSampleInterval(Time interval)
{
    NS_ABORT_MSG_IF(interval.IsStrictlyNegative(), "LinkStateSampleInterval must be >= 0");
    m_sampleInterval = interval;
}

Time
NtnTraceWriter::GetLinkStateSampleInterval() const
{
    return m_sampleInterval;
}

void
NtnTraceWriter::OpenLinkStateFile(const std::string& path)
{
    NS_LOG_FUNCTION(this << path);
    m_linkStateFile.open(path);
    NS_ABORT_MSG_IF(!m_linkStateFile.is_open(), "Cannot open link-state CSV " << path);
    m_linkStateFile << std::fixed
                    << "time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,"
                       "associated\n";
}

void
NtnTraceWriter::OpenEventLogFile(const std::string& path)
{
    NS_LOG_FUNCTION(this << path);
    m_eventLogFile.open(path);
    NS_ABORT_MSG_IF(!m_eventLogFile.is_open(), "Cannot open event log " << path);
    m_eventLogFile << std::fixed
                   << "time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail\n";
}

void
NtnTraceWriter::Connect(Ptr<NtnLinkManager> manager)
{
    NS_LOG_FUNCTION(this << manager);
    NS_ABORT_MSG_IF(!manager, "Link manager must not be null");
    NS_ABORT_MSG_IF(m_manager, "NtnTraceWriter is already connected to a link manager");
    m_manager = manager;
    if (m_linkStateFile.is_open())
    {
        manager->TraceConnectWithoutContext(
            "UpdateCompleted",
            MakeCallback(&NtnTraceWriter::HandleUpdateCompleted, this));
    }
    if (m_eventLogFile.is_open())
    {
        manager->TraceConnectWithoutContext(
            "VisibilityChange",
            MakeCallback(&NtnTraceWriter::HandleVisibilityChange, this));
        manager->TraceConnectWithoutContext(
            "AssociationChange",
            MakeCallback(&NtnTraceWriter::HandleAssociationChange, this));
        manager->TraceConnectWithoutContext(
            "SecondaryAssociationChange",
            MakeCallback(&NtnTraceWriter::HandleSecondaryAssociationChange, this));
        manager->TraceConnectWithoutContext(
            "GatewayVisibilityChange",
            MakeCallback(&NtnTraceWriter::HandleGatewayVisibilityChange, this));
        manager->TraceConnectWithoutContext(
            "GatewayAssociationChange",
            MakeCallback(&NtnTraceWriter::HandleGatewayAssociationChange, this));
    }
}

void
NtnTraceWriter::HandleUpdateCompleted(Time now)
{
    if (now < m_nextSampleTime)
    {
        return;
    }
    m_nextSampleTime = now + m_sampleInterval;
    for (const auto& pair : m_manager->GetPairStates())
    {
        WriteLinkStateRow(now, pair);
    }
    for (const auto& pair : m_manager->GetGatewayPairStates())
    {
        WriteLinkStateRow(now, pair);
    }
}

void
NtnTraceWriter::WriteLinkStateRow(Time now, const NtnPairState& pair)
{
    constexpr double MS_PER_S = 1e3;
    m_linkStateFile << std::setprecision(TIME_DECIMALS) << now.GetSeconds() << ',' << pair.groundId
                    << ',' << pair.satelliteId << ',' << (pair.visible ? 1 : 0) << ','
                    << std::setprecision(VALUE_DECIMALS) << pair.elevationDeg << ','
                    << pair.slantRangeM / M_PER_KM << ',' << pair.linkDelay.GetSeconds() * MS_PER_S
                    << ',' << (pair.associated ? 1 : 0) << '\n';
}

void
NtnTraceWriter::HandleVisibilityChange(uint32_t groundId, uint32_t satelliteId, bool visible)
{
    const NtnPairState pair = m_manager->GetPairState(groundId, satelliteId);
    m_eventLogFile << std::setprecision(TIME_DECIMALS) << Simulator::Now().GetSeconds() << ','
                   << MODULE_NAME << ",VisibilityChange," << groundId << ',' << satelliteId << ",,"
                   << std::setprecision(VALUE_DECIMALS) << pair.elevationDeg << ','
                   << pair.slantRangeM / M_PER_KM << ",elevation_deg|range_km,"
                   << (visible ? "visible" : "not_visible") << '\n';
}

void
NtnTraceWriter::HandleAssociationChange(uint32_t groundId,
                                        int32_t oldSatelliteId,
                                        int32_t newSatelliteId)
{
    // detail: "acquired" (none -> satellite), "lost" (satellite -> none) or "handover".
    std::string detail = "handover";
    if (oldSatelliteId == ntn::NO_SATELLITE)
    {
        detail = "acquired";
    }
    else if (newSatelliteId == ntn::NO_SATELLITE)
    {
        detail = "lost";
    }
    m_eventLogFile << std::setprecision(TIME_DECIMALS) << Simulator::Now().GetSeconds() << ','
                   << MODULE_NAME << ",AssociationChange," << groundId << ',' << newSatelliteId
                   << ",," << oldSatelliteId << ',';
    if (newSatelliteId != ntn::NO_SATELLITE)
    {
        constexpr double MS_PER_S = 1e3;
        const NtnPairState pair =
            m_manager->GetPairState(groundId, static_cast<uint32_t>(newSatelliteId));
        m_eventLogFile << std::setprecision(VALUE_DECIMALS)
                       << pair.linkDelay.GetSeconds() * MS_PER_S;
    }
    m_eventLogFile << ",old_satellite_id|delay_ms," << detail << '\n';
}

void
NtnTraceWriter::HandleSecondaryAssociationChange(uint32_t groundId,
                                                 uint32_t satelliteId,
                                                 bool associated)
{
    constexpr double MS_PER_S = 1e3;
    const NtnPairState pair = m_manager->GetPairState(groundId, satelliteId);
    m_eventLogFile << std::setprecision(TIME_DECIMALS) << Simulator::Now().GetSeconds() << ','
                   << MODULE_NAME << ",SecondaryAssociationChange," << groundId << ','
                   << satelliteId << ",," << std::setprecision(VALUE_DECIMALS) << pair.elevationDeg
                   << ',' << pair.linkDelay.GetSeconds() * MS_PER_S << ",elevation_deg|delay_ms,"
                   << (associated ? "added" : "removed") << '\n';
}

void
NtnTraceWriter::HandleGatewayVisibilityChange(uint32_t gatewayId,
                                              uint32_t satelliteId,
                                              bool visible)
{
    const NtnPairState pair = m_manager->GetGatewayPairState(gatewayId, satelliteId);
    m_eventLogFile << std::setprecision(TIME_DECIMALS) << Simulator::Now().GetSeconds() << ','
                   << MODULE_NAME << ",GatewayVisibilityChange," << gatewayId << ',' << satelliteId
                   << ",," << std::setprecision(VALUE_DECIMALS) << pair.elevationDeg << ','
                   << pair.slantRangeM / M_PER_KM << ",elevation_deg|range_km,"
                   << (visible ? "visible" : "not_visible") << '\n';
}

void
NtnTraceWriter::HandleGatewayAssociationChange(uint32_t satelliteId,
                                               int32_t oldGatewayId,
                                               int32_t newGatewayId)
{
    // Same layout as AssociationChange, with the satellite as node and the gateway as peer.
    std::string detail = "handover";
    if (oldGatewayId == ntn::NO_SATELLITE)
    {
        detail = "acquired";
    }
    else if (newGatewayId == ntn::NO_SATELLITE)
    {
        detail = "lost";
    }
    m_eventLogFile << std::setprecision(TIME_DECIMALS) << Simulator::Now().GetSeconds() << ','
                   << MODULE_NAME << ",GatewayAssociationChange," << satelliteId << ','
                   << newGatewayId << ",," << oldGatewayId << ',';
    if (newGatewayId != ntn::NO_SATELLITE)
    {
        constexpr double MS_PER_S = 1e3;
        const NtnPairState pair =
            m_manager->GetGatewayPairState(static_cast<uint32_t>(newGatewayId), satelliteId);
        m_eventLogFile << std::setprecision(VALUE_DECIMALS)
                       << pair.linkDelay.GetSeconds() * MS_PER_S;
    }
    m_eventLogFile << ",old_gateway_id|delay_ms," << detail << '\n';
}

} // namespace ns3
