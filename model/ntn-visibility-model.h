/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_VISIBILITY_MODEL_H
#define NTN_VISIBILITY_MODEL_H

#include "ns3/mobility-model.h"
#include "ns3/object.h"
#include "ns3/vector.h"

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Decides whether a satellite can be seen from a ground terminal.
 *
 * Geometry used (all positions Earth-centred Cartesian, metres):
 * - d = s - g is the line of sight from ground g to satellite s; its length is the slant range.
 * - g / |g| is the local "straight up" direction at the ground terminal (spherical Earth).
 * - The elevation is the angle between the line of sight and the local horizon, so
 *   sin(elevation) = (d . g/|g|) / |d|  (the "upward part" of the line of sight).
 * - The satellite is visible when elevation >= MinElevation. Elevation 0 means "on the
 *   horizon"; a negative elevation means the Earth is in the way.
 */
class NtnVisibilityModel : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnVisibilityModel();
    ~NtnVisibilityModel() override;

    /**
     * @param minElevationDeg minimum elevation for visibility [deg], 0 - 45
     */
    void SetMinElevation(double minElevationDeg);

    /** @return the minimum elevation for visibility [deg] */
    double GetMinElevation() const;

    /**
     * @param ground ground terminal mobility model
     * @param satellite satellite mobility model
     * @return the current elevation of the satellite seen from the ground [deg]
     */
    double GetElevationDeg(Ptr<const MobilityModel> ground,
                           Ptr<const MobilityModel> satellite) const;

    /**
     * @param ground ground terminal mobility model
     * @param satellite satellite mobility model
     * @return the current straight-line ground-satellite distance [m]
     */
    double GetSlantRangeM(Ptr<const MobilityModel> ground,
                          Ptr<const MobilityModel> satellite) const;

    /**
     * @param ground ground terminal mobility model
     * @param satellite satellite mobility model
     * @return true if the satellite's current elevation is at least MinElevation
     */
    bool IsVisible(Ptr<const MobilityModel> ground, Ptr<const MobilityModel> satellite) const;

    /**
     * @param elevationDeg an elevation angle [deg]
     * @return true if a satellite at this elevation counts as visible
     */
    bool IsVisibleAtElevation(double elevationDeg) const;

    /**
     * Elevation of a satellite seen from a ground point (pure geometry, no simulation time).
     * @param groundM ground position [m]; must not be the Earth centre
     * @param satelliteM satellite position [m]; must differ from groundM
     * @return elevation [deg], from -90 (straight down) to +90 (straight up)
     */
    static double ComputeElevationDeg(const Vector& groundM, const Vector& satelliteM);

    /**
     * @param groundM ground position [m]
     * @param satelliteM satellite position [m]
     * @return straight-line distance between the two points [m]
     */
    static double ComputeSlantRangeM(const Vector& groundM, const Vector& satelliteM);

  private:
    double m_minElevationDeg; //!< Minimum elevation for visibility [deg]
};

} // namespace ns3

#endif // NTN_VISIBILITY_MODEL_H
