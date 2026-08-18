// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation clocks GPU model definition.

#include "device_clocks_gpu_model.h"

#include <device_clocks.h>

#include "clock_mode_model.h"

DeviceClocksGpuModel::DeviceClocksGpuModel(const std::shared_ptr<devtrace::DeviceClocksManager>& manager, const std::shared_ptr<devtrace::DeviceClockGpu>& gpu)
    : manager_(manager)
    , gpu_(gpu)
{
    for (const auto& mode : gpu_->GetClockModes())
    {
        if (mode.type == devtrace::ClockModeType::kPeak)
        {
            continue;
        }

        clock_modes_.push_back(std::make_shared<ClockModeModel>(mode));
    }
}

QString DeviceClocksGpuModel::GetGpuName() const
{
    return gpu_->GetName().c_str();
}

const QVector<std::shared_ptr<ClockModeModel>>& DeviceClocksGpuModel::GetModeModels()
{
    return clock_modes_;
}

int DeviceClocksGpuModel::GetCurrentMode()
{
    const auto current_clock_mode_type = gpu_->QueryClockMode();
    for (int index = 0; index < static_cast<int>(clock_modes_.size()); ++index)
    {
        if (clock_modes_[index]->GetType() == current_clock_mode_type)
        {
            return index;
        }
    }

    return -1;
}

bool DeviceClocksGpuModel::RequestMode(const int index)
{
    const bool success = manager_->RequestMode(gpu_->GetId(), clock_modes_[index]->GetType());
    if (success)
    {
        emit ClockModeChanged(index);
    }

    return success;
}
