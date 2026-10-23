/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "circular-orbit-mobility-model.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CircularOrbitMobilityModel");

NS_OBJECT_ENSURE_REGISTERED(CircularOrbitMobilityModel);

TypeId
CircularOrbitMobilityModel::GetTypeId()
{
    // Largest accepted value of an angle given in [0, 360): the double just below 360. Kept
    // local because GetTypeId() already runs during static initialisation.
    const double MAX_PHASE_DEG = std::nextafter(ntn::FULL_CIRCLE_DEG, 0.0);
    static TypeId tid =
        TypeId("ns3::CircularOrbitMobilityModel")
            .SetParent<MobilityModel>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<CircularOrbitMobilityModel>()
            .AddAttribute(
                "OrbitAltitude",
                "Orbit altitude above the Earth surface [m] (project range "
                "200,000 - 2,000,000 m).",
                DoubleValue(ntn::DEFAULT_ORBIT_ALTITUDE_M),
                MakeDoubleAccessor(&CircularOrbitMobilityModel::SetOrbitAltitude,
                                   &CircularOrbitMobilityModel::GetOrbitAltitude),
                MakeDoubleChecker<double>(ntn::MIN_ORBIT_ALTITUDE_M, ntn::MAX_ORBIT_ALTITUDE_M))
            .AddAttribute(
                "OrbitInclination",
                "Tilt of the orbit plane relative to the equator [deg] (0 - 90).",
                DoubleValue(ntn::DEFAULT_INCLINATION_DEG),
                MakeDoubleAccessor(&CircularOrbitMobilityModel::SetInclination,
                                   &CircularOrbitMobilityModel::GetInclination),
                MakeDoubleChecker<double>(ntn::MIN_INCLINATION_DEG, ntn::MAX_INCLINATION_DEG))
            .AddAttribute("InitialPhase",
                          "Satellite position on its orbit at time 0, measured from the "
                          "ascending node [deg] ([0, 360)).",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&CircularOrbitMobilityModel::SetInitialPhase,
                                             &CircularOrbitMobilityModel::GetInitialPhase),
                          MakeDoubleChecker<double>(0.0, MAX_PHASE_DEG))
            .AddAttribute("Raan",
                          "Right ascension of the ascending node: the direction the orbit "
                          "plane faces [deg] ([0, 360)).",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&CircularOrbitMobilityModel::SetRaan,
                                             &CircularOrbitMobilityModel::GetRaan),
                          MakeDoubleChecker<double>(0.0, MAX_PHASE_DEG));
    return tid;
}

CircularOrbitMobilityModel::CircularOrbitMobilityModel()
    : m_altitudeM(ntn::DEFAULT_ORBIT_ALTITUDE_M),
      m_inclinationRad(ntn::DegToRad(ntn::DEFAULT_INCLINATION_DEG)),
      m_initialPhaseRad(0.0),
      m_raanRad(0.0)
{
    NS_LOG_FUNCTION(this);
}

CircularOrbitMobilityModel::~CircularOrbitMobilityModel()
{
    NS_LOG_FUNCTION(this);
}

void
CircularOrbitMobilityModel::SetOrbitAltitude(double altitudeM)
{
    NS_LOG_FUNCTION(this << altitudeM);
    NS_ABORT_MSG_IF(altitudeM < ntn::MIN_ORBIT_ALTITUDE_M || altitudeM > ntn::MAX_ORBIT_ALTITUDE_M,
                    "OrbitAltitude " << altitudeM << " m is outside the project range "
                                     << ntn::MIN_ORBIT_ALTITUDE_M << " - "
                                     << ntn::MAX_ORBIT_ALTITUDE_M << " m");
    m_altitudeM = altitudeM;
}

double
CircularOrbitMobilityModel::GetOrbitAltitude() const
{
    return m_altitudeM;
}

void
CircularOrbitMobilityModel::SetInclination(double inclinationDeg)
{
    NS_LOG_FUNCTION(this << inclinationDeg);
    NS_ABORT_MSG_IF(inclinationDeg < ntn::MIN_INCLINATION_DEG ||
                        inclinationDeg > ntn::MAX_INCLINATION_DEG,
                    "OrbitInclination " << inclinationDeg << " deg is outside the range "
                                        << ntn::MIN_INCLINATION_DEG << " - "
                                        << ntn::MAX_INCLINATION_DEG << " deg");
    m_inclinationRad = ntn::DegToRad(inclinationDeg);
}

double
CircularOrbitMobilityModel::GetInclination() const
{
    return ntn::RadToDeg(m_inclinationRad);
}

void
CircularOrbitMobilityModel::SetInitialPhase(double phaseDeg)
{
    NS_LOG_FUNCTION(this << phaseDeg);
    NS_ABORT_MSG_IF(phaseDeg < 0.0 || phaseDeg >= ntn::FULL_CIRCLE_DEG,
                    "InitialPhase " << phaseDeg << " deg is outside the range [0, 360)");
    m_initialPhaseRad = ntn::DegToRad(phaseDeg);
}

double
CircularOrbitMobilityModel::GetInitialPhase() const
{
    return ntn::RadToDeg(m_initialPhaseRad);
}

void
CircularOrbitMobilityModel::SetRaan(double raanDeg)
{
    NS_LOG_FUNCTION(this << raanDeg);
    NS_ABORT_MSG_IF(raanDeg < 0.0 || raanDeg >= ntn::FULL_CIRCLE_DEG,
                    "Raan " << raanDeg << " deg is outside the range [0, 360)");
    m_raanRad = ntn::DegToRad(raanDeg);
}

double
CircularOrbitMobilityModel::GetRaan() const
{
    return ntn::RadToDeg(m_raanRad);
}

double
CircularOrbitMobilityModel::GetOrbitRadiusM() const
{
    return ntn::EARTH_RADIUS_M + m_altitudeM;
}

double
CircularOrbitMobilityModel::GetAngularVelocityRadPerS() const
{
    // w = 2*pi / T = sqrt(mu / a^3): a higher orbit is both longer and slower.
    const double radiusM = GetOrbitRadiusM();
    return std::sqrt(ntn::EARTH_GRAVITATIONAL_PARAMETER_M3_PER_S2 / (radiusM * radiusM * radiusM));
}

Time
CircularOrbitMobilityModel::GetOrbitPeriod() const
{
    return Seconds(2.0 * M_PI / GetAngularVelocityRadPerS());
}

double
CircularOrbitMobilityModel::GetPhaseRad(Time t) const
{
    return m_initialPhaseRad + GetAngularVelocityRadPerS() * t.GetSeconds();
}

Vector
CircularOrbitMobilityModel::GetPositionAt(Time t) const
{
    // Step 1: position inside the orbit plane. The satellite moves on a circle of radius a;
    // after phase u it is at (a*cos u, a*sin u, 0), with the x axis pointing to the ascending
    // node (where it crosses the equator going north).
    // Step 2: tilt that plane by the inclination i (rotation about the x axis), which lifts the
    // in-plane y component partly into z: (x, y*cos i, y*sin i).
    // Step 3: turn the tilted ring about the Earth axis (z) by the RAAN, which moves the
    // ascending node to longitude RAAN.
    // Multiplying the three steps out gives the formulas below.
    const double radiusM = GetOrbitRadiusM();
    const double u = GetPhaseRad(t);
    const double cosU = std::cos(u);
    const double sinU = std::sin(u);
    const double cosI = std::cos(m_inclinationRad);
    const double sinI = std::sin(m_inclinationRad);
    const double cosRaan = std::cos(m_raanRad);
    const double sinRaan = std::sin(m_raanRad);

    return Vector(radiusM * (cosU * cosRaan - sinU * cosI * sinRaan),
                  radiusM * (cosU * sinRaan + sinU * cosI * cosRaan),
                  radiusM * (sinU * sinI));
}

Vector
CircularOrbitMobilityModel::GetVelocityAt(Time t) const
{
    // Differentiate GetPositionAt() with du/dt = w: cos u becomes -w*sin u and sin u becomes
    // w*cos u. The speed is a*w, and the velocity is always perpendicular to the position.
    const double speedMps = GetOrbitRadiusM() * GetAngularVelocityRadPerS();
    const double u = GetPhaseRad(t);
    const double cosU = std::cos(u);
    const double sinU = std::sin(u);
    const double cosI = std::cos(m_inclinationRad);
    const double sinI = std::sin(m_inclinationRad);
    const double cosRaan = std::cos(m_raanRad);
    const double sinRaan = std::sin(m_raanRad);

    return Vector(speedMps * (-sinU * cosRaan - cosU * cosI * sinRaan),
                  speedMps * (-sinU * sinRaan + cosU * cosI * cosRaan),
                  speedMps * (cosU * sinI));
}

Vector
CircularOrbitMobilityModel::DoGetPosition() const
{
    return GetPositionAt(Simulator::Now());
}

Vector
CircularOrbitMobilityModel::DoGetVelocity() const
{
    return GetVelocityAt(Simulator::Now());
}

void
CircularOrbitMobilityModel::DoSetPosition(const Vector& position)
{
    NS_FATAL_ERROR("CircularOrbitMobilityModel does not support SetPosition("
                   << position << "): the position follows from OrbitAltitude, "
                   << "OrbitInclination, Raan, InitialPhase and the simulation time");
}

int64_t
CircularOrbitMobilityModel::DoAssignStreams(int64_t /* stream */)
{
    return 0;
}

} // namespace ns3
