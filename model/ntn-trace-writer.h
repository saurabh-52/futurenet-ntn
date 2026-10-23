/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NTN_TRACE_WRITER_H
#define NTN_TRACE_WRITER_H

#include "ntn-link-manager.h"

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <fstream>
#include <string>

namespace ns3
{

/**
 * @ingroup futurenet-ntn
 * Writes the module's machine-readable output files from NtnLinkManager traces.
 *
 * 1. Link-state CSV (SRS section 9), one row per ground-satellite pair per sample:
 *    time_s,ground_id,satellite_id,visible,elevation_deg,range_km,delay_ms,associated
 *    delay_ms is the one-way link delay (propagation + processing). With gateways, every
 *    gateway-satellite pair gets a row too, after the ground rows: ground_id then holds the
 *    gateway node ID and `associated` means "the satellite uses this gateway".
 * 2. Common event log (Common Guidelines section 4.5), one row per event:
 *    time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail
 *    The `unit` column names what value1 and value2 mean, separated by '|'. Events:
 *    VisibilityChange, AssociationChange, SecondaryAssociationChange, GatewayVisibilityChange,
 *    GatewayAssociationChange and IslStateChange.
 * 3. ISL CSV (Phase 3, optional), one row per ISL that is *up* per sample (writing every
 *    satellite pair would give S^2/2 rows per sample):
 *    time_s,satellite_a,satellite_b,distance_km,clearance_km,delay_ms
 *
 * Every file is optional; the ISL CSV uses the same sample interval as the link-state CSV. Output
 * is fully deterministic (fixed number formatting), so two runs with the same configuration produce
 * byte-identical files.
 */
class NtnTraceWriter : public Object
{
  public:
    /**
     * Register this type.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    NtnTraceWriter();
    ~NtnTraceWriter() override;

    /**
     * Open the link-state CSV and write its header. Call before Connect().
     * @param path output file path
     */
    void OpenLinkStateFile(const std::string& path);

    /**
     * Open the common event log and write its header. Call before Connect().
     * @param path output file path
     */
    void OpenEventLogFile(const std::string& path);

    /**
     * Open the ISL CSV and write its header. Call before Connect().
     * @param path output file path
     */
    void OpenIslFile(const std::string& path);

    /**
     * Start writing rows from a link manager's traces.
     * @param manager the link manager to listen to
     */
    void Connect(Ptr<NtnLinkManager> manager);

    /** @param interval minimum time between link-state samples (0 = every update) */
    void SetLinkStateSampleInterval(Time interval);

    /** @return the minimum time between link-state samples */
    Time GetLinkStateSampleInterval() const;

  protected:
    void DoDispose() override;

  private:
    /**
     * UpdateCompleted handler: writes the link-state rows when a sample is due.
     * @param now time of the finished update
     */
    void HandleUpdateCompleted(Time now);

    /**
     * VisibilityChange handler.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param visible new visibility
     */
    void HandleVisibilityChange(uint32_t groundId, uint32_t satelliteId, bool visible);

    /**
     * AssociationChange handler.
     * @param groundId ground terminal node ID
     * @param oldSatelliteId previous satellite (-1 = none)
     * @param newSatelliteId new satellite (-1 = none)
     */
    void HandleAssociationChange(uint32_t groundId, int32_t oldSatelliteId, int32_t newSatelliteId);

    /**
     * SecondaryAssociationChange handler.
     * @param groundId ground terminal node ID
     * @param satelliteId satellite node ID
     * @param associated true if added to the secondary associations, false if removed
     */
    void HandleSecondaryAssociationChange(uint32_t groundId, uint32_t satelliteId, bool associated);

    /**
     * GatewayVisibilityChange handler.
     * @param gatewayId gateway node ID
     * @param satelliteId satellite node ID
     * @param visible new visibility
     */
    void HandleGatewayVisibilityChange(uint32_t gatewayId, uint32_t satelliteId, bool visible);

    /**
     * GatewayAssociationChange handler.
     * @param satelliteId satellite node ID
     * @param oldGatewayId previous gateway (-1 = none)
     * @param newGatewayId new gateway (-1 = none)
     */
    void HandleGatewayAssociationChange(uint32_t satelliteId,
                                        int32_t oldGatewayId,
                                        int32_t newGatewayId);

    /**
     * IslStateChange handler.
     * @param satelliteA lower satellite node ID
     * @param satelliteB higher satellite node ID
     * @param up true if the ISL came up, false if it went down
     */
    void HandleIslStateChange(uint32_t satelliteA, uint32_t satelliteB, bool up);

    /**
     * Write one link-state CSV row.
     * @param now sample time
     * @param pair the pair (ground terminal or gateway with a satellite)
     */
    void WriteLinkStateRow(Time now, const NtnPairState& pair);

    Ptr<NtnLinkManager> m_manager; //!< Manager being followed
    std::ofstream m_linkStateFile; //!< Link-state CSV
    std::ofstream m_eventLogFile;  //!< Common event log CSV
    std::ofstream m_islFile;       //!< ISL CSV
    Time m_sampleInterval;         //!< Minimum time between link-state samples
    Time m_nextSampleTime;         //!< Earliest time of the next link-state sample
};

} // namespace ns3

#endif // NTN_TRACE_WRITER_H
