/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-propagation-delay-model.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnPropagationDelayModel");

NS_OBJECT_ENSURE_REGISTERED(NtnPropagationDelayModel);

TypeId
NtnPropagationDelayModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnPropagationDelayModel")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnPropagationDelayModel>()
            .AddAttribute("PropagationSpeed",
                          "Signal propagation speed [m/s]. Default: speed of light in vacuum "
                          "(299,792,458 m/s); allowed range 1 m/s up to that value.",
                          DoubleValue(ntn::SPEED_OF_LIGHT_M_PER_S),
                          MakeDoubleAccessor(&NtnPropagationDelayModel::SetPropagationSpeed,
                                             &NtnPropagationDelayModel::GetPropagationSpeed),
                          MakeDoubleChecker<double>(ntn::MIN_PROPAGATION_SPEED_M_PER_S,
                                                    ntn::SPEED_OF_LIGHT_M_PER_S))
            .AddAttribute("ProcessingDelay",
                          "Fixed delay added to every link on top of the propagation delay "
                          "(project range 0 - 100 ms).",
                          TimeValue(MilliSeconds(ntn::DEFAULT_PROCESSING_DELAY_MS)),
                          MakeTimeAccessor(&NtnPropagationDelayModel::SetProcessingDelay,
                                           &NtnPropagationDelayModel::GetProcessingDelay),
                          MakeTimeChecker(Time(0), MilliSeconds(ntn::MAX_PROCESSING_DELAY_MS)));
    return tid;
}

NtnPropagationDelayModel::NtnPropagationDelayModel()
    : m_propagationSpeedMps(ntn::SPEED_OF_LIGHT_M_PER_S),
      m_processingDelay(MilliSeconds(ntn::DEFAULT_PROCESSING_DELAY_MS))
{
    NS_LOG_FUNCTION(this);
}

NtnPropagationDelayModel::~NtnPropagationDelayModel()
{
    NS_LOG_FUNCTION(this);
}

void
NtnPropagationDelayModel::SetPropagationSpeed(double speedMps)
{
    NS_LOG_FUNCTION(this << speedMps);
    NS_ABORT_MSG_IF(speedMps < ntn::MIN_PROPAGATION_SPEED_M_PER_S ||
                        speedMps > ntn::SPEED_OF_LIGHT_M_PER_S,
                    "PropagationSpeed " << speedMps << " m/s must be within "
                                        << ntn::MIN_PROPAGATION_SPEED_M_PER_S << " - "
                                        << ntn::SPEED_OF_LIGHT_M_PER_S << " m/s");
    m_propagationSpeedMps = speedMps;
}

double
NtnPropagationDelayModel::GetPropagationSpeed() const
{
    return m_propagationSpeedMps;
}

void
NtnPropagationDelayModel::SetProcessingDelay(Time delay)
{
    NS_LOG_FUNCTION(this << delay);
    NS_ABORT_MSG_IF(delay.IsStrictlyNegative() ||
                        delay > MilliSeconds(ntn::MAX_PROCESSING_DELAY_MS),
                    "ProcessingDelay " << delay.As(Time::MS) << " is outside 0 - "
                                       << ntn::MAX_PROCESSING_DELAY_MS << " ms");
    m_processingDelay = delay;
}

Time
NtnPropagationDelayModel::GetProcessingDelay() const
{
    return m_processingDelay;
}

Time
NtnPropagationDelayModel::GetPropagationDelay(double pathLengthM) const
{
    NS_ABORT_MSG_IF(pathLengthM < 0.0, "Path length " << pathLengthM << " m is negative");
    return Seconds(pathLengthM / m_propagationSpeedMps);
}

Time
NtnPropagationDelayModel::GetLinkDelay(double pathLengthM) const
{
    return GetPropagationDelay(pathLengthM) + m_processingDelay;
}

} // namespace ns3
