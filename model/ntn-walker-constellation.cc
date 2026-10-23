/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-walker-constellation.h"

#include "ntn-constants.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NtnWalkerConstellation");

NS_OBJECT_ENSURE_REGISTERED(NtnWalkerConstellation);

TypeId
NtnWalkerConstellation::GetTypeId()
{
    // Largest accepted value of an angle given in [0, 360): the double just below 360.
    const double MAX_PHASE_DEG = std::nextafter(ntn::FULL_CIRCLE_DEG, 0.0);
    // Smallest accepted RaanSpread: the double just above 0 (all planes at one RAAN is not a
    // multi-plane constellation).
    const double MIN_SPREAD_DEG = std::nextafter(0.0, 1.0);
    static TypeId tid =
        TypeId("ns3::NtnWalkerConstellation")
            .SetParent<Object>()
            .SetGroupName("FuturenetNtn")
            .AddConstructor<NtnWalkerConstellation>()
            .AddAttribute("NumOrbitalPlanes",
                          "Number of orbital planes; the satellites are divided equally between "
                          "them (project range 1 - 20).",
                          UintegerValue(ntn::DEFAULT_NUM_ORBITAL_PLANES),
                          MakeUintegerAccessor(&NtnWalkerConstellation::SetNumOrbitalPlanes,
                                               &NtnWalkerConstellation::GetNumOrbitalPlanes),
                          MakeUintegerChecker<uint32_t>(ntn::MIN_NUM_ORBITAL_PLANES,
                                                        ntn::MAX_NUM_ORBITAL_PLANES))
            .AddAttribute("PlanePhaseOffset",
                          "Extra initial phase of each plane relative to the previous plane "
                          "[deg] ([0, 360)). Walker i:N/P/F uses F * 360 / N.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&NtnWalkerConstellation::SetPlanePhaseOffset,
                                             &NtnWalkerConstellation::GetPlanePhaseOffset),
                          MakeDoubleChecker<double>(0.0, MAX_PHASE_DEG))
            .AddAttribute("RaanSpread",
                          "Angle over which the plane RAANs are spread evenly [deg] ((0, 360]). "
                          "360 = Walker delta, 180 = Walker star.",
                          DoubleValue(ntn::FULL_CIRCLE_DEG),
                          MakeDoubleAccessor(&NtnWalkerConstellation::SetRaanSpread,
                                             &NtnWalkerConstellation::GetRaanSpread),
                          MakeDoubleChecker<double>(MIN_SPREAD_DEG, ntn::FULL_CIRCLE_DEG));
    return tid;
}

NtnWalkerConstellation::NtnWalkerConstellation()
    : m_numPlanes(ntn::DEFAULT_NUM_ORBITAL_PLANES),
      m_planePhaseOffsetDeg(0.0),
      m_raanSpreadDeg(ntn::FULL_CIRCLE_DEG)
{
    NS_LOG_FUNCTION(this);
}

NtnWalkerConstellation::~NtnWalkerConstellation()
{
    NS_LOG_FUNCTION(this);
}

void
NtnWalkerConstellation::SetNumOrbitalPlanes(uint32_t numPlanes)
{
    NS_LOG_FUNCTION(this << numPlanes);
    NS_ABORT_MSG_IF(
        numPlanes < ntn::MIN_NUM_ORBITAL_PLANES || numPlanes > ntn::MAX_NUM_ORBITAL_PLANES,
        "NumOrbitalPlanes " << numPlanes << " is outside the project range "
                            << ntn::MIN_NUM_ORBITAL_PLANES << " - " << ntn::MAX_NUM_ORBITAL_PLANES);
    m_numPlanes = numPlanes;
}

uint32_t
NtnWalkerConstellation::GetNumOrbitalPlanes() const
{
    return m_numPlanes;
}

void
NtnWalkerConstellation::SetPlanePhaseOffset(double offsetDeg)
{
    NS_LOG_FUNCTION(this << offsetDeg);
    NS_ABORT_MSG_IF(offsetDeg < 0.0 || offsetDeg >= ntn::FULL_CIRCLE_DEG,
                    "PlanePhaseOffset " << offsetDeg << " deg is outside the range [0, 360)");
    m_planePhaseOffsetDeg = offsetDeg;
}

double
NtnWalkerConstellation::GetPlanePhaseOffset() const
{
    return m_planePhaseOffsetDeg;
}

void
NtnWalkerConstellation::SetRaanSpread(double spreadDeg)
{
    NS_LOG_FUNCTION(this << spreadDeg);
    NS_ABORT_MSG_IF(spreadDeg <= 0.0 || spreadDeg > ntn::FULL_CIRCLE_DEG,
                    "RaanSpread " << spreadDeg << " deg is outside the range (0, 360]");
    m_raanSpreadDeg = spreadDeg;
}

double
NtnWalkerConstellation::GetRaanSpread() const
{
    return m_raanSpreadDeg;
}

NtnOrbitSlot
NtnWalkerConstellation::GetSlot(uint32_t index,
                                uint32_t numSatellites,
                                double baseRaanDeg,
                                double basePhaseDeg) const
{
    NS_ABORT_MSG_IF(numSatellites == 0 || index >= numSatellites,
                    "Satellite index " << index << " is outside 0 - " << numSatellites);
    NS_ABORT_MSG_IF(numSatellites % m_numPlanes != 0,
                    numSatellites << " satellites cannot be divided equally between " << m_numPlanes
                                  << " orbital planes; use a multiple of NumOrbitalPlanes");
    const uint32_t perPlane = numSatellites / m_numPlanes;
    NtnOrbitSlot slot;
    slot.planeIndex = index / perPlane;
    slot.slotInPlane = index % perPlane;
    const double planeRaanDeg = slot.planeIndex * m_raanSpreadDeg / m_numPlanes;
    // Same evaluation order as the Phase 1 single-ring layout, so one plane gives bit-identical
    // phases.
    const double spacingDeg = ntn::FULL_CIRCLE_DEG / perPlane;
    const double inPlanePhaseDeg = slot.slotInPlane * spacingDeg;
    const double planePhaseDeg = slot.planeIndex * m_planePhaseOffsetDeg;
    slot.raanDeg = std::fmod(baseRaanDeg + planeRaanDeg, ntn::FULL_CIRCLE_DEG);
    slot.initialPhaseDeg =
        std::fmod(basePhaseDeg + inPlanePhaseDeg + planePhaseDeg, ntn::FULL_CIRCLE_DEG);
    return slot;
}

} // namespace ns3
