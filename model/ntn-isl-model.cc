/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-isl-model.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnIslModel");

NS_OBJECT_ENSURE_REGISTERED(NtnIslModel);

TypeId
NtnIslModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnIslModel")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnIslModel>()
            .AddAttribute(
                "MaxIslRange",
                "Longest usable inter-satellite link [m] (project range "
                "100,000 - 20,000,000 m).",
                DoubleValue(ntn::DEFAULT_ISL_RANGE_M),
                MakeDoubleAccessor(&NtnIslModel::SetMaxIslRange, &NtnIslModel::GetMaxIslRange),
                MakeDoubleChecker<double>(ntn::MIN_ISL_RANGE_M, ntn::MAX_ISL_RANGE_M))
            .AddAttribute("IslMinClearance",
                          "Lowest altitude the straight line between two satellites may reach "
                          "for an ISL [m] (0 = it only must not pass through the Earth; project "
                          "range 0 - 1,000,000 m).",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&NtnIslModel::SetIslMinClearance,
                                             &NtnIslModel::GetIslMinClearance),
                          MakeDoubleChecker<double>(0.0, ntn::MAX_ISL_CLEARANCE_M));
    return tid;
}

NtnIslModel::NtnIslModel()
    : m_maxIslRangeM(ntn::DEFAULT_ISL_RANGE_M),
      m_minClearanceM(0.0)
{
    NS_LOG_FUNCTION(this);
}

NtnIslModel::~NtnIslModel()
{
    NS_LOG_FUNCTION(this);
}

void
NtnIslModel::SetMaxIslRange(double rangeM)
{
    NS_LOG_FUNCTION(this << rangeM);
    NS_ABORT_MSG_IF(rangeM < ntn::MIN_ISL_RANGE_M || rangeM > ntn::MAX_ISL_RANGE_M,
                    "MaxIslRange " << rangeM << " m is outside the project range "
                                   << ntn::MIN_ISL_RANGE_M << " - " << ntn::MAX_ISL_RANGE_M
                                   << " m");
    m_maxIslRangeM = rangeM;
}

double
NtnIslModel::GetMaxIslRange() const
{
    return m_maxIslRangeM;
}

void
NtnIslModel::SetIslMinClearance(double clearanceM)
{
    NS_LOG_FUNCTION(this << clearanceM);
    NS_ABORT_MSG_IF(clearanceM < 0.0 || clearanceM > ntn::MAX_ISL_CLEARANCE_M,
                    "IslMinClearance " << clearanceM << " m is outside the project range 0 - "
                                       << ntn::MAX_ISL_CLEARANCE_M << " m");
    m_minClearanceM = clearanceM;
}

double
NtnIslModel::GetIslMinClearance() const
{
    return m_minClearanceM;
}

double
NtnIslModel::ComputeLineClearanceM(const Vector& aM, const Vector& bM)
{
    const Vector abM = bM - aM;
    const double lengthSquared = abM * abM; // ns-3 Vector '*' is the dot product
    NS_ABORT_MSG_IF(lengthSquared <= 0.0, "The two satellites are at the same point");
    // Parameter of the point on the line A + t (B - A) closest to the Earth centre, kept on the
    // segment.
    const double t = std::clamp(-(aM * abM) / lengthSquared, 0.0, 1.0);
    const Vector closestM = aM + t * abM;
    return closestM.GetLength() - ntn::EARTH_RADIUS_M;
}

bool
NtnIslModel::IsLinkPossible(double distanceM, double clearanceM) const
{
    return distanceM <= m_maxIslRangeM && clearanceM >= m_minClearanceM;
}

} // namespace ns3
