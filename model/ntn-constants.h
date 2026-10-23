/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_CONSTANTS_H
#define NTN_CONSTANTS_H

#include <cmath>
#include <cstdint>

/**
 * @file
 * @ingroup futurenet-ntn
 * Physical constants and project configuration ranges used by the futurenet-ntn module.
 *
 * The ranges below are the *project configuration ranges* of the M01 SRS (section 7). They were
 * chosen for teaching and testing; they are NOT values mandated by 3GPP or any other standard.
 */

namespace ns3
{
namespace ntn
{

/// Mean Earth radius [m]. The model uses a spherical Earth with this radius. It matches the
/// sphere used by ns3::GeographicPositions, so ground positions and orbits share one Earth.
constexpr double EARTH_RADIUS_M = 6371000.0;

/// Earth's standard gravitational parameter mu = G * M_earth [m^3/s^2]. It fixes how fast a
/// satellite must move to stay on a circular orbit of a given radius.
constexpr double EARTH_GRAVITATIONAL_PARAMETER_M3_PER_S2 = 3.986004418e14;

/// Speed of light in vacuum [m/s] (exact by definition of the metre).
constexpr double SPEED_OF_LIGHT_M_PER_S = 299792458.0;

/// Value used for "no satellite" wherever a satellite node ID is expected.
constexpr int32_t NO_SATELLITE = -1;

/// Degrees in a full circle; used to wrap angles and spread satellites around an orbit.
constexpr double FULL_CIRCLE_DEG = 360.0;

// ---- Project configuration ranges (SRS section 7) ----

constexpr double MIN_ORBIT_ALTITUDE_M = 200e3;        ///< Lowest allowed orbit altitude [m]
constexpr double MAX_ORBIT_ALTITUDE_M = 2000e3;       ///< Highest allowed orbit altitude [m]
constexpr double DEFAULT_ORBIT_ALTITUDE_M = 600e3;    ///< Default orbit altitude [m]
constexpr double MIN_INCLINATION_DEG = 0.0;           ///< Lowest orbit inclination [deg]
constexpr double MAX_INCLINATION_DEG = 90.0;          ///< Highest orbit inclination [deg]
constexpr double DEFAULT_INCLINATION_DEG = 53.0;      ///< Default orbit inclination [deg]
constexpr double MIN_MIN_ELEVATION_DEG = 0.0;         ///< Lowest allowed MinElevation [deg]
constexpr double MAX_MIN_ELEVATION_DEG = 45.0;        ///< Highest allowed MinElevation [deg]
constexpr double DEFAULT_MIN_ELEVATION_DEG = 10.0;    ///< Default MinElevation [deg]
constexpr double MAX_GROUND_HEIGHT_M = 5000.0;        ///< Highest ground terminal height [m]
constexpr double MAX_LATITUDE_DEG = 90.0;             ///< Latitude limit (+/-) [deg]
constexpr double MAX_LONGITUDE_DEG = 180.0;           ///< Longitude limit (+/-) [deg]
constexpr uint32_t MIN_NUM_SATELLITES = 1;            ///< Fewest satellites per constellation
constexpr uint32_t MAX_NUM_SATELLITES = 200;          ///< Most satellites per constellation
constexpr uint32_t MIN_NUM_ORBITAL_PLANES = 1;        ///< Fewest orbital planes
constexpr uint32_t MAX_NUM_ORBITAL_PLANES = 20;       ///< Most orbital planes
constexpr uint32_t DEFAULT_NUM_ORBITAL_PLANES = 1;    ///< Default number of orbital planes
constexpr int64_t MIN_UPDATE_INTERVAL_MS = 10;        ///< Shortest UpdateInterval [ms]
constexpr int64_t MAX_UPDATE_INTERVAL_MS = 10000;     ///< Longest UpdateInterval [ms]
constexpr int64_t DEFAULT_UPDATE_INTERVAL_MS = 100;   ///< Default UpdateInterval [ms]
constexpr uint64_t MIN_DATA_RATE_BPS = 1000000;       ///< Lowest link DataRate (1 Mbit/s)
constexpr uint64_t MAX_DATA_RATE_BPS = 10000000000;   ///< Highest link DataRate (10 Gbit/s)
constexpr uint64_t DEFAULT_DATA_RATE_BPS = 100000000; ///< Default link DataRate (100 Mbit/s)
constexpr int64_t MAX_PROCESSING_DELAY_MS = 100;      ///< Longest ProcessingDelay [ms]
constexpr int64_t DEFAULT_PROCESSING_DELAY_MS = 1;    ///< Default ProcessingDelay [ms]
constexpr double MIN_PROPAGATION_SPEED_M_PER_S = 1.0; ///< Slowest allowed PropagationSpeed [m/s]

/**
 * Convert an angle from degrees (used in configuration and output) to radians (used inside
 * the maths).
 * @param angleDeg angle [deg]
 * @return the same angle [rad]
 */
inline double
DegToRad(double angleDeg)
{
    return angleDeg * M_PI / 180.0;
}

/**
 * Convert an angle from radians to degrees.
 * @param angleRad angle [rad]
 * @return the same angle [deg]
 */
inline double
RadToDeg(double angleRad)
{
    return angleRad * 180.0 / M_PI;
}

} // namespace ntn
} // namespace ns3

#endif // NTN_CONSTANTS_H
