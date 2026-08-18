// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for object that provides the active GPU.

#include "active_gpu_provider.h"
#include "rgp_pal_client_info.h"

#include <algorithm>

static constexpr uint32_t kTargetGpuPollRateMs = 1000;

namespace devtrace
{
    ActiveGpuProvider::ActiveGpuProvider(DDDriverUtilsApi* driver_utils_api, const SystemGpuInfo& system_gpu_info, const DDConnectionId umd_connection_id)
        : driver_utils_api_(driver_utils_api)
        , gpus_(system_gpu_info.gpus)
        , spm_supported_gpus_(system_gpu_info.spm_supported_gpus)
        , umd_connection_id_(umd_connection_id)
        , active_gpu_{}
    {
    }

    void ActiveGpuProvider::StartPolling(ActiveGpuEvent event)
    {
        thread_ = std::thread([this, event] {
            while (!exit_thread_semaphore_.try_acquire())
            {
                std::scoped_lock lock(active_gpu_mutex_);
                if (const system_info_utils::GpuInfo info = QueryActiveGpu(); info.name != active_gpu_.name)
                {
                    active_gpu_ = info;
                    event.callback(event.listener, active_gpu_);
                }
            }
        });
    }

    void ActiveGpuProvider::StopPolling()
    {
        exit_thread_semaphore_.release();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    system_info_utils::GpuInfo ActiveGpuProvider::GetActiveGpu()
    {
        std::scoped_lock lock(active_gpu_mutex_);
        return active_gpu_;
    }

    bool ActiveGpuProvider::DoesGpuSupportSpm(const system_info_utils::GpuInfo& gpu) const
    {
        const auto find = std::ranges::find_if(spm_supported_gpus_, [&](const auto& candidate) { return candidate.name == gpu.name; });
        return find != spm_supported_gpus_.end();
    }

    system_info_utils::GpuInfo ActiveGpuProvider::QueryActiveGpu()
    {
        if (gpus_.empty())
        {
            return {};
        }

        if (const auto pal_client_info = PalClientInfo::Query(driver_utils_api_, umd_connection_id_); pal_client_info != nullptr)
        {
            if (system_info_utils::GpuInfo gpu = pal_client_info->GetActiveGpu(gpus_); !gpu.name.empty())
            {
                return gpu;
            }
        }

        // As a fallback just use the GPU with the most CUs
        return *std::ranges::max_element(gpus_, [](const auto& lhs, const auto& rhs) { return lhs.asic.num_cus < rhs.asic.num_cus; });
    }
}  // namespace devtrace
