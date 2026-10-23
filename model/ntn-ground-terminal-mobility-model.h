/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_GROUND_TERMINAL_MOBILITY_MODEL_H
#define NTN_GROUND_TERMINAL_MOBILITY_MODEL_H

#include "ns3/mobility-model.h"

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Fixed ground terminal placed by latitude, longitude and height on a spherical Earth.
 *
 * The position is returned in the same Earth-centred Cartesian frame as
 * CircularOrbitMobilityModel. The conversion is done by ns3::GeographicPositions using its
 * spherical Earth model (radius 6,371 km). Earth rotation is ignored, so the terminal does not
 * move and its velocity is zero.
 *
 * A moving ground terminal can use any other ns-3 MobilityModel that reports positions in
 * this Earth-centred frame.
 */
class NtnGroundTerminalMobilityModel : public MobilityModel
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnGroundTerminalMobilityModel();
    ~NtnGroundTerminalMobilityModel() override;

    /**
     * @return an independent copy of this model at the same location
     */
    Ptr<MobilityModel> Copy() const override
    {
        return CreateObject<NtnGroundTerminalMobilityModel>(*this);
    }

    /**
     * @param latitudeDeg latitude [deg], -90 (South Pole) to +90 (North Pole)
     */
    void SetLatitude(double latitudeDeg);

    /** @return the latitude [deg] */
    double GetLatitude() const;

    /**
     * @param longitudeDeg longitude [deg], -180 to +180 (positive = east)
     */
    void SetLongitude(double longitudeDeg);

    /** @return the longitude [deg] */
    double GetLongitude() const;

    /**
     * @param heightM height above the Earth surface [m], 0 - 5,000 m
     */
    void SetGroundHeight(double heightM);

    /** @return the height above the Earth surface [m] */
    double GetGroundHeight() const;

  private:
    Vector DoGetPosition() const override;
    Vector DoGetVelocity() const override;
    /**
     * Not supported: place the terminal with the Latitude, Longitude and GroundHeight
     * Attributes instead. Calling it stops the simulation with an explanation.
     * @param position ignored
     */
    void DoSetPosition(const Vector& position) override;

    /** Recompute the cached Cartesian position after a coordinate change. */
    void UpdateCartesianPosition();

    double m_latitudeDeg;  //!< Latitude [deg]
    double m_longitudeDeg; //!< Longitude [deg]
    double m_heightM;      //!< Height above the Earth surface [m]
    Vector m_positionM;    //!< Cached Earth-centred Cartesian position [m]
};

} // namespace ns3

#endif // NTN_GROUND_TERMINAL_MOBILITY_MODEL_H
