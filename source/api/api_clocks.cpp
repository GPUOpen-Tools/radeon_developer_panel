// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for clock mode handler for the capture API.

#include "api_clocks.h"

#include <device_clocks.h>

#include "api_allocator.h"

void ApiClocks::OnGpuEvent(void* object, const devtrace::DeviceClockGpuInfo& gpu_info)
{
    auto* self = static_cast<ApiClocks*>(object);
    self->ConvertGpus(gpu_info);
}

ApiClocks::ApiClocks(const std::shared_ptr<devtrace::DeviceClocksManager>& device_clocks_manager)
    : device_clocks_manager_(device_clocks_manager)
{
    device_clocks_manager_->RegisterEvent({.listener = this, .callback = &OnGpuEvent});
}

void ApiClocks::ConvertGpus(const devtrace::DeviceClockGpuInfo& gpu_info)
{
    for (const auto& gpu : gpu_info.gpus)
    {
        gpus_.push_back({});
        ClockGpuDetails& details = gpus_.back();
        details.gpu_id           = gpu->GetId();

        for (const devtrace::ClockMode& mode : gpu->GetClockModes())
        {
            RdpCaptureGpuClockModeDetails api_mode{};
            api_mode.mode = static_cast<RdpCaptureGpuClockMode>(mode.type);

            api_mode.min_gpu_freq = mode.min_gpu_freq;
            api_mode.max_gpu_freq = mode.max_gpu_freq;

            api_mode.min_mem_freq = mode.min_mem_freq;
            api_mode.max_mem_freq = mode.max_mem_freq;

            details.api_modes.emplace_back(api_mode);
        }
    }
}

void ApiClocks::GetGpuClockModes(const uint64_t gpu_index, RdpCaptureGpuClockModeDetails** modes, uint64_t* num_modes) const
{
    if (gpu_index >= gpus_.size())
    {
        *modes     = nullptr;
        *num_modes = 0;
    }

    const auto& [gpu_id, api_modes] = gpus_[gpu_index];
    const size_t buffer_size        = sizeof(RdpCaptureGpuClockModeDetails) * api_modes.size();

    *modes     = static_cast<RdpCaptureGpuClockModeDetails*>(ApiAlloc(buffer_size));
    *num_modes = api_modes.size();

    memcpy(*modes, api_modes.data(), buffer_size);
}

RdpCaptureResult ApiClocks::QueryGpuCurrentClockMode(const uint64_t gpu_index, RdpCaptureGpuClockMode* mode) const
{
    if (gpu_index >= gpus_.size())
    {
        return kRdpCaptureResultInvalidParams;
    }

    const ClockGpuDetails& gpu_details = gpus_[gpu_index];
    *mode                              = static_cast<RdpCaptureGpuClockMode>(device_clocks_manager_->QueryClockMode(gpu_details.gpu_id));

    return kRdpCaptureResultSuccess;
}

RdpCaptureResult ApiClocks::SetCurrentGpuClockMode(const uint64_t gpu_index, RdpCaptureGpuClockMode mode) const
{
    if (gpu_index >= gpus_.size())
    {
        return kRdpCaptureResultInvalidParams;
    }

    const ClockGpuDetails& gpu_details = gpus_[gpu_index];
    return device_clocks_manager_->RequestMode(gpu_details.gpu_id, static_cast<devtrace::ClockModeType>(mode)) ? kRdpCaptureResultSuccess
                                                                                                               : kRdpCaptureResultFailure;
}
