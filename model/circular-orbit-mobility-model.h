/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef CIRCULAR_ORBIT_MOBILITY_MODEL_H
#define CIRCULAR_ORBIT_MOBILITY_MODEL_H

#include "ns3/mobility-model.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Mobility model of a satellite on a circular orbit around a spherical Earth.
 *
 * Positions are Earth-centred Cartesian coordinates in metres (origin at the centre of the
 * Earth, z axis through the North Pole, x axis through latitude 0 / longitude 0). Earth rotation
 * is ignored, so the orbit is fixed relative to the ground.
 *
 * The orbit is described by:
 * - OrbitAltitude h: height above the Earth surface, so the orbit radius is a = R + h;
 * - OrbitInclination i: tilt of the orbit plane relative to the equator;
 * - Raan (right ascension of the ascending node): which way the tilted orbit ring faces, i.e.
 *   the longitude where the satellite crosses the equator going north;
 * - InitialPhase u0: where on the ring the satellite is at time 0, measured from that crossing.
 *
 * Orbit period (SRS section 7, option (a)): a circular orbit needs gravity to supply exactly the
 * centripetal force, which gives T = 2*pi*sqrt(a^3 / mu) and angular speed w = 2*pi / T.
 *
 * The position is a pure function of time (see GetPositionAt()); the model keeps no motion
 * state, so the same time always gives the same position.
 */
class CircularOrbitMobilityModel : public MobilityModel
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    CircularOrbitMobilityModel();
    ~CircularOrbitMobilityModel() override;

    /**
     * @return an independent copy of this model with the same orbit
     */
    Ptr<MobilityModel> Copy() const override
    {
        return CreateObject<CircularOrbitMobilityModel>(*this);
    }

    /**
     * Set the orbit altitude above the Earth surface.
     * @param altitudeM altitude [m], must be within 200,000 - 2,000,000 m
     */
    void SetOrbitAltitude(double altitudeM);

    /** @return the orbit altitude [m] */
    double GetOrbitAltitude() const;

    /**
     * Set the orbit inclination.
     * @param inclinationDeg inclination [deg], must be within 0 - 90
     */
    void SetInclination(double inclinationDeg);

    /** @return the orbit inclination [deg] */
    double GetInclination() const;

    /**
     * Set the satellite position on the orbit ring at time 0.
     * @param phaseDeg initial phase [deg], must be within [0, 360)
     */
    void SetInitialPhase(double phaseDeg);

    /** @return the initial phase [deg] */
    double GetInitialPhase() const;

    /**
     * Set the right ascension of the ascending node (direction the orbit plane faces).
     * @param raanDeg RAAN [deg], must be within [0, 360)
     */
    void SetRaan(double raanDeg);

    /** @return the RAAN [deg] */
    double GetRaan() const;

    /** @return the orbit radius a = EarthRadius + altitude [m] */
    double GetOrbitRadiusM() const;

    /** @return the orbit period T = 2*pi*sqrt(a^3 / mu) */
    Time GetOrbitPeriod() const;

    /** @return the angular speed w = 2*pi / T [rad/s] */
    double GetAngularVelocityRadPerS() const;

    /**
     * Satellite position at any simulation time. A pure function: it does not depend on the
     * current simulation time or on any earlier call.
     * @param t simulation time
     * @return Earth-centred Cartesian position [m]
     */
    Vector GetPositionAt(Time t) const;

    /**
     * Satellite velocity at any simulation time (time derivative of GetPositionAt()).
     * @param t simulation time
     * @return Earth-centred Cartesian velocity [m/s]
     */
    Vector GetVelocityAt(Time t) const;

  private:
    Vector DoGetPosition() const override;
    Vector DoGetVelocity() const override;
    /**
     * Not supported: an orbit position follows from the orbit parameters and time. Calling it
     * stops the simulation with an explanation.
     * @param position ignored
     */
    void DoSetPosition(const Vector& position) override;
    /**
     * The model is deterministic and uses no random variables.
     * @param stream first stream index
     * @return 0 (no streams used)
     */
    int64_t DoAssignStreams(int64_t stream) override;

    /**
     * Phase angle u(t) = u0 + w*t: how far the satellite has travelled around the ring since
     * the ascending node.
     * @param t simulation time
     * @return phase [rad]
     */
    double GetPhaseRad(Time t) const;

    double m_altitudeM;       //!< Orbit altitude above the Earth surface [m]
    double m_inclinationRad;  //!< Orbit inclination [rad]
    double m_initialPhaseRad; //!< Phase at time 0 [rad]
    double m_raanRad;         //!< Right ascension of the ascending node [rad]
};

} // namespace ns3

#endif // CIRCULAR_ORBIT_MOBILITY_MODEL_H
