/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_PROPAGATION_DELAY_MODEL_H
#define NTN_PROPAGATION_DELAY_MODEL_H

#include "ns3/nstime.h"
#include "ns3/object.h"

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Turns a path length into a one-way link delay.
 *
 *   propagation delay = path length / PropagationSpeed
 *   link delay        = propagation delay + ProcessingDelay
 *
 * ProcessingDelay is a fixed extra delay standing in for everything that is not travel time
 * (e.g. on-board handling). Queueing and transmission time are not included here: the ns-3
 * NetDevice that carries the packets adds those.
 */
class NtnPropagationDelayModel : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnPropagationDelayModel();
    ~NtnPropagationDelayModel() override;

    /**
     * @param speedMps propagation speed [m/s], > 0 and at most the speed of light
     */
    void SetPropagationSpeed(double speedMps);

    /** @return the propagation speed [m/s] */
    double GetPropagationSpeed() const;

    /**
     * @param delay additive processing delay, 0 - 100 ms
     */
    void SetProcessingDelay(Time delay);

    /** @return the additive processing delay */
    Time GetProcessingDelay() const;

    /**
     * @param pathLengthM path length [m], >= 0
     * @return path length / PropagationSpeed
     */
    Time GetPropagationDelay(double pathLengthM) const;

    /**
     * @param pathLengthM path length [m], >= 0
     * @return propagation delay + ProcessingDelay
     */
    Time GetLinkDelay(double pathLengthM) const;

  private:
    double m_propagationSpeedMps; //!< Propagation speed [m/s]
    Time m_processingDelay;       //!< Additive processing delay
};

} // namespace ns3

#endif // NTN_PROPAGATION_DELAY_MODEL_H
