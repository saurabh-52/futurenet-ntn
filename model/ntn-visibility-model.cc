/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-visibility-model.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnVisibilityModel");

NS_OBJECT_ENSURE_REGISTERED(NtnVisibilityModel);

TypeId
NtnVisibilityModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnVisibilityModel")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnVisibilityModel>()
            .AddAttribute(
                "MinElevation",
                "Minimum elevation angle at which a satellite counts as visible from a "
                "ground terminal [deg] (project range 0 - 45 deg).",
                DoubleValue(ntn::DEFAULT_MIN_ELEVATION_DEG),
                MakeDoubleAccessor(&NtnVisibilityModel::SetMinElevation,
                                   &NtnVisibilityModel::GetMinElevation),
                MakeDoubleChecker<double>(ntn::MIN_MIN_ELEVATION_DEG, ntn::MAX_MIN_ELEVATION_DEG));
    return tid;
}

NtnVisibilityModel::NtnVisibilityModel()
    : m_minElevationDeg(ntn::DEFAULT_MIN_ELEVATION_DEG)
{
    NS_LOG_FUNCTION(this);
}

NtnVisibilityModel::~NtnVisibilityModel()
{
    NS_LOG_FUNCTION(this);
}

void
NtnVisibilityModel::SetMinElevation(double minElevationDeg)
{
    NS_LOG_FUNCTION(this << minElevationDeg);
    NS_ABORT_MSG_IF(minElevationDeg < ntn::MIN_MIN_ELEVATION_DEG ||
                        minElevationDeg > ntn::MAX_MIN_ELEVATION_DEG,
                    "MinElevation " << minElevationDeg << " deg is outside the project range "
                                    << ntn::MIN_MIN_ELEVATION_DEG << " - "
                                    << ntn::MAX_MIN_ELEVATION_DEG << " deg");
    m_minElevationDeg = minElevationDeg;
}

double
NtnVisibilityModel::GetMinElevation() const
{
    return m_minElevationDeg;
}

double
NtnVisibilityModel::ComputeElevationDeg(const Vector& groundM, const Vector& satelliteM)
{
    const double groundRadiusM = groundM.GetLength();
    NS_ABORT_MSG_IF(groundRadiusM <= 0.0,
                    "Ground position is the Earth centre; positions must be Earth-centred "
                    "Cartesian coordinates of a point on or above the surface");
    const Vector lineOfSightM = satelliteM - groundM;
    const double slantRangeM = lineOfSightM.GetLength();
    NS_ABORT_MSG_IF(slantRangeM <= 0.0, "Ground terminal and satellite are at the same point");

    // Component of the line of sight along the local vertical ("how much of it points up"),
    // divided by its length = sine of the elevation angle.
    const double upwardM =
        (lineOfSightM.x * groundM.x + lineOfSightM.y * groundM.y + lineOfSightM.z * groundM.z) /
        groundRadiusM;
    // Clamp guards against rounding pushing the ratio a hair outside [-1, 1].
    const double sinElevation = std::clamp(upwardM / slantRangeM, -1.0, 1.0);
    return ntn::RadToDeg(std::asin(sinElevation));
}

double
NtnVisibilityModel::ComputeSlantRangeM(const Vector& groundM, const Vector& satelliteM)
{
    return CalculateDistance(groundM, satelliteM);
}

double
NtnVisibilityModel::GetElevationDeg(Ptr<const MobilityModel> ground,
                                    Ptr<const MobilityModel> satellite) const
{
    return ComputeElevationDeg(ground->GetPosition(), satellite->GetPosition());
}

double
NtnVisibilityModel::GetSlantRangeM(Ptr<const MobilityModel> ground,
                                   Ptr<const MobilityModel> satellite) const
{
    return ComputeSlantRangeM(ground->GetPosition(), satellite->GetPosition());
}

bool
NtnVisibilityModel::IsVisibleAtElevation(double elevationDeg) const
{
    return elevationDeg >= m_minElevationDeg;
}

bool
NtnVisibilityModel::IsVisible(Ptr<const MobilityModel> ground,
                              Ptr<const MobilityModel> satellite) const
{
    return IsVisibleAtElevation(GetElevationDeg(ground, satellite));
}

} // namespace ns3
