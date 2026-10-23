/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_ATTRIBUTE_CHECKERS_H
#define NTN_ATTRIBUTE_CHECKERS_H

#include "ns3/attribute.h"
#include "ns3/data-rate.h"

/**
 * @file
 * @ingroup futurenet-ntn
 * Attribute checkers that ns-3 does not provide out of the box.
 */

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Create a checker that accepts a DataRate only inside [minRate, maxRate].
 *
 * ns-3 offers bounded checkers for numbers and Time, but MakeDataRateChecker() accepts any
 * value. With this checker an out-of-range DataRate is rejected by the Attribute system itself:
 * SetAttribute() stops with an error and SetAttributeFailSafe() returns false (CG-11).
 *
 * @param minRate smallest accepted rate (inclusive)
 * @param maxRate largest accepted rate (inclusive)
 * @return the checker
 */
Ptr<const AttributeChecker> MakeNtnDataRateChecker(DataRate minRate, DataRate maxRate);

} // namespace ns3

#endif // NTN_ATTRIBUTE_CHECKERS_H
