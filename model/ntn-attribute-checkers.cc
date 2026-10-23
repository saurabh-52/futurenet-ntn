/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ntn-attribute-checkers.h"

#include <sstream>

namespace ns3
{

namespace
{

/**
 * DataRate checker with an inclusive [min, max] range. It behaves like the stock
 * DataRateChecker (same value type, same copy rules) but additionally rejects values outside
 * the range.
 */
class NtnBoundedDataRateChecker : public DataRateChecker
{
  public:
    /**
     * @param minRate smallest accepted rate
     * @param maxRate largest accepted rate
     */
    NtnBoundedDataRateChecker(DataRate minRate, DataRate maxRate)
        : m_minRate(minRate),
          m_maxRate(maxRate)
    {
    }

    bool Check(const AttributeValue& value) const override
    {
        const auto* rate = dynamic_cast<const DataRateValue*>(&value);
        return rate != nullptr && rate->Get() >= m_minRate && rate->Get() <= m_maxRate;
    }

    std::string GetValueTypeName() const override
    {
        return "ns3::DataRateValue";
    }

    bool HasUnderlyingTypeInformation() const override
    {
        return true;
    }

    std::string GetUnderlyingTypeInformation() const override
    {
        std::ostringstream oss;
        oss << "DataRate " << m_minRate << ":" << m_maxRate;
        return oss.str();
    }

    Ptr<AttributeValue> Create() const override
    {
        return ns3::Create<DataRateValue>();
    }

    bool Copy(const AttributeValue& source, AttributeValue& destination) const override
    {
        const auto* src = dynamic_cast<const DataRateValue*>(&source);
        auto* dst = dynamic_cast<DataRateValue*>(&destination);
        if (src == nullptr || dst == nullptr)
        {
            return false;
        }
        *dst = *src;
        return true;
    }

  private:
    DataRate m_minRate; //!< Smallest accepted rate
    DataRate m_maxRate; //!< Largest accepted rate
};

} // namespace

Ptr<const AttributeChecker>
MakeNtnDataRateChecker(DataRate minRate, DataRate maxRate)
{
    return Create<NtnBoundedDataRateChecker>(minRate, maxRate);
}

} // namespace ns3
