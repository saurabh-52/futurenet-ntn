/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-ground-terminal-mobility-model.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/geographic-positions.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnGroundTerminalMobilityModel");

NS_OBJECT_ENSURE_REGISTERED(NtnGroundTerminalMobilityModel);

TypeId
NtnGroundTerminalMobilityModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NtnGroundTerminalMobilityModel")
            .SetParent<MobilityModel>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnGroundTerminalMobilityModel>()
            .AddAttribute("Latitude",
                          "Latitude of the ground terminal [deg] (-90 to +90, positive = north).",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&NtnGroundTerminalMobilityModel::SetLatitude,
                                             &NtnGroundTerminalMobilityModel::GetLatitude),
                          MakeDoubleChecker<double>(-ntn::MAX_LATITUDE_DEG, ntn::MAX_LATITUDE_DEG))
            .AddAttribute(
                "Longitude",
                "Longitude of the ground terminal [deg] (-180 to +180, positive = east).",
                DoubleValue(0.0),
                MakeDoubleAccessor(&NtnGroundTerminalMobilityModel::SetLongitude,
                                   &NtnGroundTerminalMobilityModel::GetLongitude),
                MakeDoubleChecker<double>(-ntn::MAX_LONGITUDE_DEG, ntn::MAX_LONGITUDE_DEG))
            .AddAttribute("GroundHeight",
                          "Height of the ground terminal above the Earth surface [m] "
                          "(project range 0 - 5,000 m).",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&NtnGroundTerminalMobilityModel::SetGroundHeight,
                                             &NtnGroundTerminalMobilityModel::GetGroundHeight),
                          MakeDoubleChecker<double>(0.0, ntn::MAX_GROUND_HEIGHT_M));
    return tid;
}

NtnGroundTerminalMobilityModel::NtnGroundTerminalMobilityModel()
    : m_latitudeDeg(0.0),
      m_longitudeDeg(0.0),
      m_heightM(0.0)
{
    NS_LOG_FUNCTION(this);
    UpdateCartesianPosition();
}

NtnGroundTerminalMobilityModel::~NtnGroundTerminalMobilityModel()
{
    NS_LOG_FUNCTION(this);
}

void
NtnGroundTerminalMobilityModel::SetLatitude(double latitudeDeg)
{
    NS_LOG_FUNCTION(this << latitudeDeg);
    NS_ABORT_MSG_IF(std::abs(latitudeDeg) > ntn::MAX_LATITUDE_DEG,
                    "Latitude " << latitudeDeg << " deg is outside -90 to +90 deg");
    m_latitudeDeg = latitudeDeg;
    UpdateCartesianPosition();
}

double
NtnGroundTerminalMobilityModel::GetLatitude() const
{
    return m_latitudeDeg;
}

void
NtnGroundTerminalMobilityModel::SetLongitude(double longitudeDeg)
{
    NS_LOG_FUNCTION(this << longitudeDeg);
    NS_ABORT_MSG_IF(std::abs(longitudeDeg) > ntn::MAX_LONGITUDE_DEG,
                    "Longitude " << longitudeDeg << " deg is outside -180 to +180 deg");
    m_longitudeDeg = longitudeDeg;
    UpdateCartesianPosition();
}

double
NtnGroundTerminalMobilityModel::GetLongitude() const
{
    return m_longitudeDeg;
}

void
NtnGroundTerminalMobilityModel::SetGroundHeight(double heightM)
{
    NS_LOG_FUNCTION(this << heightM);
    NS_ABORT_MSG_IF(heightM < 0.0 || heightM > ntn::MAX_GROUND_HEIGHT_M,
                    "GroundHeight " << heightM << " m is outside the project range 0 - "
                                    << ntn::MAX_GROUND_HEIGHT_M << " m");
    m_heightM = heightM;
    UpdateCartesianPosition();
}

double
NtnGroundTerminalMobilityModel::GetGroundHeight() const
{
    return m_heightM;
}

void
NtnGroundTerminalMobilityModel::UpdateCartesianPosition()
{
    // Reuse ns-3's converter with its spherical Earth (same radius as ntn::EARTH_RADIUS_M), so
    // the terminal ends up at distance (R + height) from the Earth centre.
    m_positionM =
        GeographicPositions::GeographicToCartesianCoordinates(m_latitudeDeg,
                                                              m_longitudeDeg,
                                                              m_heightM,
                                                              GeographicPositions::SPHERE);
}

Vector
NtnGroundTerminalMobilityModel::DoGetPosition() const
{
    return m_positionM;
}

Vector
NtnGroundTerminalMobilityModel::DoGetVelocity() const
{
    // Earth rotation is not modelled, so a fixed terminal does not move in this frame.
    return Vector(0.0, 0.0, 0.0);
}

void
NtnGroundTerminalMobilityModel::DoSetPosition(const Vector& position)
{
    NS_FATAL_ERROR("NtnGroundTerminalMobilityModel does not support SetPosition("
                   << position << "): set the Latitude, Longitude and GroundHeight attributes");
}

} // namespace ns3
