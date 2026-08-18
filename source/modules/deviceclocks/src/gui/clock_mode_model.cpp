// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks clock mode model implementation

#include "clock_mode_model.h"

#include <QJsonDocument>
#include <utility>

static constexpr int  kMHzConversionFactor = 1000000;
static constexpr auto kMhzSuffix           = "MHz";

static QString HzToMHzString(const uint64_t hz_freq)
{
    return QString::number(static_cast<double>(hz_freq) / kMHzConversionFactor);
}

ClockModeModel::ClockModeModel(devtrace::ClockMode clock_mode)
    : clock_mode_(std::move(clock_mode))
{
}

QString ClockModeModel::GetName() const
{
    return clock_mode_.name.c_str();
}

QString ClockModeModel::GetDescription() const
{
    return clock_mode_.desc.c_str();
}

devtrace::ClockModeType ClockModeModel::GetType() const
{
    return clock_mode_.type;
}

QString ClockModeModel::GetGpuFreqStr() const
{
    return GetFreqStr(clock_mode_.min_gpu_freq, clock_mode_.max_gpu_freq);
}

QString ClockModeModel::GetMemFreqStr() const
{
    return GetFreqStr(clock_mode_.min_mem_freq, clock_mode_.max_mem_freq);
}

QString ClockModeModel::GetFreqStr(const uint64_t min, const uint64_t max)
{
    if (min == max)
    {
        return QString("%1 %2").arg(HzToMHzString(min), kMhzSuffix);
    }

    return QString("%1-%2 %3").arg(HzToMHzString(min), HzToMHzString(max), kMhzSuffix);
}
