/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_ISL_MODEL_H
#define NTN_ISL_MODEL_H

#include "ns3/object.h"
#include "ns3/vector.h"

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Simplified inter-satellite link (ISL) feasibility rule (SRS M01-FR-12).
 *
 * Two satellites can have an ISL when
 * 1. their distance is at most MaxIslRange (stands in for terminal power / pointing limits), and
 * 2. the straight line between them stays at least IslMinClearance above the Earth surface, i.e.
 *    the Earth (plus an optional margin, e.g. for the atmosphere) does not block the line of
 *    sight.
 *
 * Geometry of rule 2: for the segment from A to B, the point closest to the Earth centre is
 * P(t) = A + t (B - A) with t = -A.(B - A) / |B - A|^2 clamped to [0, 1] (if the closest point of
 * the infinite line lies outside the segment, an end point is the closest). The clearance is
 * |P(t)| - EarthRadius: the lowest altitude the line reaches. Negative means the line passes
 * through the Earth.
 *
 * No antenna, pointing, Doppler or link budget is modelled; a feasible ISL is always usable.
 */
class NtnIslModel : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnIslModel();
    ~NtnIslModel() override;

    /** @param rangeM longest usable ISL [m], 100,000 - 20,000,000 */
    void SetMaxIslRange(double rangeM);

    /** @return the longest usable ISL [m] */
    double GetMaxIslRange() const;

    /** @param clearanceM lowest allowed altitude of the ISL line of sight [m], 0 - 1,000,000 */
    void SetIslMinClearance(double clearanceM);

    /** @return the lowest allowed altitude of the ISL line of sight [m] */
    double GetIslMinClearance() const;

    /**
     * Lowest altitude reached by the straight line between two points (see class description).
     * @param aM first point, Earth-centred Cartesian [m]
     * @param bM second point, Earth-centred Cartesian [m]
     * @return clearance above the Earth surface [m]; negative if the line passes through the Earth
     */
    static double ComputeLineClearanceM(const Vector& aM, const Vector& bM);

    /**
     * @param distanceM satellite-satellite distance [m]
     * @param clearanceM line-of-sight clearance from ComputeLineClearanceM() [m]
     * @return true if an ISL is possible
     */
    bool IsLinkPossible(double distanceM, double clearanceM) const;

  private:
    double m_maxIslRangeM;  //!< Longest usable ISL [m]
    double m_minClearanceM; //!< Lowest allowed line-of-sight altitude [m]
};

} // namespace ns3

#endif // NTN_ISL_MODEL_H
