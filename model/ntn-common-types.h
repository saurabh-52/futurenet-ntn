/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_COMMON_TYPES_H
#define NTN_COMMON_TYPES_H

#include "ns3/nstime.h"
#include "ns3/vector.h"

#include <cstdint>

/**
 * @file
 * @ingroup futurenet-ntn
 * State structures exchanged with other modules.
 *
 * NtnLinkState and NtnMobilityState mirror the FutureNet common interface contract
 * (Common Guidelines section 4.2 and 4.3) field-for-field: same field names, meanings and
 * units. No instructor-supplied futurenet-common package exists yet; when one is supplied these
 * two structs should be replaced by its LinkState / MobilityState types.
 */

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Logical link state between two nodes (mirrors the common LinkState contract).
 *
 * For a ground-satellite pair: sourceNodeId is the ground terminal, destinationNodeId the
 * satellite. `available` is true while the link carries traffic, i.e. while the ground terminal
 * is associated with that satellite.
 */
struct NtnLinkState
{
    uint32_t sourceNodeId{0};       //!< Node ID of the link source (ground terminal)
    uint32_t destinationNodeId{0};  //!< Node ID of the link destination (satellite)
    bool available{false};          //!< True if the link is currently up (associated)
    double dataRateMbps{0.0};       //!< Logical link data rate [Mbit/s]
    double propagationDelayMs{0.0}; //!< Path length / propagation speed [ms]; NaN if not visible
    double lossRatio{0.0};          //!< Packet loss ratio [0,1]; always 0 (loss is not modelled)
};

/**
 * @ingroup futurenet-ntn
 * Position and velocity snapshot of one node (mirrors the common MobilityState contract).
 */
struct NtnMobilityState
{
    uint32_t nodeId{0}; //!< Node ID
    Vector positionM;   //!< Earth-centred Cartesian position [m]
    Vector velocityMps; //!< Earth-centred Cartesian velocity [m/s]
    Time timestamp;     //!< Simulation time of the snapshot
};

/**
 * @ingroup futurenet-ntn
 * Full geometric and association state of one ground-satellite pair, as computed by the most
 * recent NtnLinkManager::Update(). This is the module-specific companion of NtnLinkState and is
 * what the link-state CSV is written from.
 */
struct NtnPairState
{
    uint32_t groundId{0};     //!< Ground terminal node ID
    uint32_t satelliteId{0};  //!< Satellite node ID
    bool visible{false};      //!< Elevation >= MinElevation
    double elevationDeg{0.0}; //!< Elevation of the satellite seen from the ground [deg]
    double slantRangeM{0.0};  //!< Straight-line ground-satellite distance [m]
    Time propagationDelay;    //!< slantRange / PropagationSpeed
    Time linkDelay;           //!< propagationDelay + ProcessingDelay (one-way link delay)
    bool associated{false};   //!< True if this satellite is the ground terminal's association
};

} // namespace ns3

#endif // NTN_COMMON_TYPES_H
