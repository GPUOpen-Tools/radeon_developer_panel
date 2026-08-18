// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for string formatting utilities.

#include "common/inc/formatting.h"

static constexpr std::array<const char*, 4> kHertzDenominations     = {"Hz", "KHz", "MHz", "GHz"};
static constexpr std::array<const char*, 6> kByteDenominations      = {"B", "KB", "MB", "GB", "TB", "PB"};
static constexpr std::array<const char*, 6> kBandwidthDenominations = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s", "PB/s"};

QString Formatting::FormatHertz(uint64_t hertz, int decimals)
{
    return Formatting::Format(hertz, 1000, kHertzDenominations, decimals);
}

QString Formatting::HzToMHz(uint64_t hertz)
{
    // FIXME(mguerret): Someday with C++20 spans could be potentially used here instead
    constexpr std::array<const char*, 3> denoms = {"Hz", "KHz", "MHz"};
    return Formatting::Format(hertz, 1000, denoms, 0);
}

QString Formatting::MHzToGHz(uint64_t m_hertz)
{
    return Formatting::Format(m_hertz, 10, kHertzDenominations, 2);
}

QString Formatting::FormatBytesPow2(uint64_t bytes, uint8_t num_decimals)
{
    return Formatting::Format(bytes, 1024, kByteDenominations, num_decimals);
}

QString Formatting::FormatBandwidth(uint64_t bytes_per_second, uint8_t num_decimals)
{
    return Formatting::Format(bytes_per_second, 1000, kBandwidthDenominations, num_decimals);
}

template <size_t LevelCount>
QString Formatting::Format(uint64_t number, uint16_t exp_base, const std::array<const char*, LevelCount>& labels, uint8_t num_decimals)
{
    uint64_t upper_bound = 1;
    for (size_t power_idx = 0; power_idx < LevelCount; power_idx++)
    {
        upper_bound = upper_bound * exp_base;
        if (number < upper_bound || power_idx == LevelCount - 1)
        {
            double unit_base = static_cast<double>(number) / static_cast<double>(upper_bound / exp_base);

            // Lowest denomination can't have a decimal
            uint8_t num_effective_decimals = power_idx != 0 ? num_decimals : 0;

            // Use the 'f' format, since this one never uses scientific notation
            return QString::number(unit_base, 'f', num_effective_decimals) + QString(" %1").arg(labels[power_idx]);
        }
    }

    return {};
}
