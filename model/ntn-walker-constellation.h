/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_WALKER_CONSTELLATION_H
#define NTN_WALKER_CONSTELLATION_H

#include "ns3/object.h"

#include <cstdint>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Where one satellite sits in a constellation: its plane, its place in the plane and the two
 * orbit angles that follow from them.
 */
struct NtnOrbitSlot
{
    uint32_t planeIndex{0};      //!< Orbital plane, 0 .. NumOrbitalPlanes-1
    uint32_t slotInPlane{0};     //!< Position in the plane, 0 .. satellitesPerPlane-1
    double raanDeg{0.0};         //!< RAAN of the plane [deg], in [0, 360)
    double initialPhaseDeg{0.0}; //!< Phase of the satellite at time 0 [deg], in [0, 360)
};

/**
 * @ingroup futurenet-ntn
 * Walker-style layout of N satellites in P orbital planes (SRS M01-FR-10).
 *
 * All planes have the same altitude and inclination; they differ only in RAAN, i.e. in which
 * direction the tilted ring faces. With S = N / P satellites per plane, satellite k is placed
 * in plane p = k / S at slot j = k mod S, and gets
 *
 *   RAAN(p)     = Raan0 + p * RaanSpread / P
 *   phase(p, j) = InitialPhase0 + j * 360 / S + p * PlanePhaseOffset
 *
 * (both wrapped into [0, 360)). Raan0 and InitialPhase0 are the orbit Attributes of the first
 * satellite. In words: the planes are fanned out evenly over RaanSpread degrees, the satellites
 * of a plane are spaced evenly around it, and each plane's satellites are shifted forward by
 * PlanePhaseOffset relative to the previous plane so that neighbouring planes do not pass over
 * the same spot at the same moment.
 *
 * RaanSpread = 360 deg gives a "Walker delta" constellation (typical for inclined orbits such as
 * 53 deg); 180 deg gives a "Walker star" (typical for near-polar orbits). In Walker's notation
 * i:N/P/F the offset is PlanePhaseOffset = F * 360 / N.
 *
 * With NumOrbitalPlanes = 1 the layout is exactly the single-ring layout of Phase 1.
 */
class NtnWalkerConstellation : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnWalkerConstellation();
    ~NtnWalkerConstellation() override;

    /** @param numPlanes number of orbital planes, 1 - 20 */
    void SetNumOrbitalPlanes(uint32_t numPlanes);

    /** @return the number of orbital planes */
    uint32_t GetNumOrbitalPlanes() const;

    /** @param offsetDeg phase shift between neighbouring planes [deg], in [0, 360) */
    void SetPlanePhaseOffset(double offsetDeg);

    /** @return the phase shift between neighbouring planes [deg] */
    double GetPlanePhaseOffset() const;

    /** @param spreadDeg angle over which the plane RAANs are spread [deg], in (0, 360] */
    void SetRaanSpread(double spreadDeg);

    /** @return the angle over which the plane RAANs are spread [deg] */
    double GetRaanSpread() const;

    /**
     * Compute the slot of one satellite. Aborts if numSatellites is not a multiple of
     * NumOrbitalPlanes (every plane must hold the same number of satellites).
     * @param index satellite index in the constellation, 0 .. numSatellites-1
     * @param numSatellites total number of satellites N
     * @param baseRaanDeg RAAN of plane 0 [deg]
     * @param basePhaseDeg phase of satellite 0 [deg]
     * @return the slot
     */
    NtnOrbitSlot GetSlot(uint32_t index,
                         uint32_t numSatellites,
                         double baseRaanDeg,
                         double basePhaseDeg) const;

  private:
    uint32_t m_numPlanes;         //!< Number of orbital planes P
    double m_planePhaseOffsetDeg; //!< Phase shift between neighbouring planes [deg]
    double m_raanSpreadDeg;       //!< Angle over which the plane RAANs are spread [deg]
};

} // namespace ns3

#endif // NTN_WALKER_CONSTELLATION_H
