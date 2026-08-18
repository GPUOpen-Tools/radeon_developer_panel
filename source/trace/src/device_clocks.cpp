// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the device clocks manager.

#include "device_clocks.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

#include <dd_connection_api.h>

#include <overlay_manager.h>
#include <system_info_cache.h>

#include "logging_definitions.h"

namespace devtrace
{
    DeviceClockGpu::DeviceClockGpu(std::string gpu_name, const system_info_utils::GpuInfo& gpu, DDClocksApi* clocks_api, std::shared_ptr<Logger> logger)
        : gpu_name_(std::move(gpu_name))
        , gpu_(gpu)
        , clocks_api_(clocks_api)
        , logger_(logger->WithSource("Device Clock GPU"))
        , gpu_id_(gpu.pci.bus << 16 | gpu.pci.device << 8 | gpu.pci.function)
    {
        uint32_t num_modes;
        if (clocks_api_->QueryClockModes(clocks_api_->pInstance, &num_modes, nullptr, gpu_id_) != DD_RESULT_SUCCESS)
        {
            logger_->LogError("Failed to query clock mode count for GPU '{}'", kLoggingInvalidPid, kLoggingInvalidUmdId, gpu_name_);
            return;
        }

        std::vector<DDDeviceClocksClockModeInfo> dd_modes(num_modes);
        if (clocks_api_->QueryClockModes(clocks_api_->pInstance, &num_modes, dd_modes.data(), gpu_id_) != DD_RESULT_SUCCESS)
        {
            logger_->LogError("Failed to query clock modes for GPU '{}'", kLoggingInvalidPid, kLoggingInvalidUmdId, gpu_name_);
            return;
        }

        Convert(dd_modes);
    }

    const std::string& DeviceClockGpu::GetName() const
    {
        return gpu_name_;
    }

    uint32_t DeviceClockGpu::GetId() const
    {
        static std::hash<std::string> hasher;
        return static_cast<uint32_t>(hasher(gpu_name_));
    }

    ClockModeType DeviceClockGpu::GetClockModeType(const char* name)
    {
        const std::string_view mode_name(name);
        if (mode_name == "Normal")
        {
            return ClockModeType::kNormal;
        }

        if (mode_name == "Stable")
        {
            return ClockModeType::kStable;
        }

        if (mode_name == "Peak")
        {
            return ClockModeType::kPeak;
        }

        return ClockModeType::kUnknown;
    }

    void DeviceClockGpu::Convert(const std::vector<DDDeviceClocksClockModeInfo>& dd_modes)
    {
        // ReSharper disable once CppUseStructuredBinding
        for (const DDDeviceClocksClockModeInfo& dd_mode : dd_modes)
        {
            ClockMode mode{};
            mode.name = dd_mode.pDescription->pName;
            mode.desc = dd_mode.pDescription->pDescription;

            mode.type    = GetClockModeType(dd_mode.pDescription->pName);
            mode.mode_id = dd_mode.pDescription->id;

            switch (mode.type)
            {
            case ClockModeType::kNormal:
                mode.min_gpu_freq = gpu_.asic.engine_clock_hz.min;
                mode.max_gpu_freq = gpu_.asic.engine_clock_hz.max;

                mode.min_mem_freq = gpu_.memory.mem_clock_hz.min;
                mode.max_mem_freq = gpu_.memory.mem_clock_hz.max;

                break;
            case ClockModeType::kStable:
            case ClockModeType::kPeak:
                mode.min_gpu_freq = dd_mode.clks.gpuClock;
                mode.max_gpu_freq = dd_mode.clks.gpuClock;

                mode.min_mem_freq = dd_mode.clks.memoryClock;
                mode.max_mem_freq = dd_mode.clks.memoryClock;
                break;
            default:
                continue;
            }

            clock_modes_.emplace_back(std::move(mode));
        }
    }

    ClockModeType DeviceClockGpu::QueryClockMode() const
    {
        DD_DEVICE_CLOCK_MODE current_clock_mode;
        if (clocks_api_->QueryCurrentClockMode(clocks_api_->pInstance, &current_clock_mode, gpu_id_) != DD_RESULT_SUCCESS)
        {
            logger_->LogError("Failed to query current clock mode for GPU '{}'", kLoggingInvalidPid, kLoggingInvalidUmdId, gpu_name_);
            return ClockModeType::kUnknown;
        }

        return static_cast<ClockModeType>(current_clock_mode);
    }

    const std::vector<ClockMode>& DeviceClockGpu::GetClockModes() const
    {
        return clock_modes_;
    }

    UpdatableDeviceClockGpu::UpdatableDeviceClockGpu(const std::string&                gpu_name,
                                                     const system_info_utils::GpuInfo& gpu,
                                                     DDClocksApi*                      clocks_api,
                                                     std::shared_ptr<Logger>           logger)
        : DeviceClockGpu(gpu_name, gpu, clocks_api, std::move(logger))
    {
    }

    bool UpdatableDeviceClockGpu::RequestMode(ClockModeType mode) const
    {
        if (const auto mode_id = static_cast<DD_DEVICE_CLOCK_MODE>(mode);
            clocks_api_->SetClockMode(clocks_api_->pInstance, mode_id, gpu_id_) != DD_RESULT_SUCCESS)
        {
            logger_->LogError("Failed to set clock mode for GPU '{}'", kLoggingInvalidPid, kLoggingInvalidUmdId, gpu_name_);
            return false;
        }

        const ClockModeType new_mode = QueryClockMode();
        return mode == new_mode;
    }

    bool UpdatableDeviceClockGpu::SetForcePeak(const bool force_peak)
    {
        if (!force_peak)
        {
            if (cached_mode_.has_value())
            {
                const ClockModeType cached_mode = cached_mode_.value();
                cached_mode_.reset();

                return RequestMode(cached_mode);
            }

            return true;
        }

        // If the GPU doesn't support peak, there's nothing to do so we report success.
        if (const auto mode = std::ranges::find_if(clock_modes_, [](const auto& c_mode) { return c_mode.type == ClockModeType::kPeak; });
            mode == clock_modes_.end())
        {
            return true;
        }

        cached_mode_ = QueryClockMode();
        return RequestMode(ClockModeType::kPeak);
    }

    void DeviceClocksManager::OnDriverConnected(DDConnectionCallbacksImpl* manager_ptr, [[maybe_unused]] const DDConnectionInfo* connection_info)
    {
        const auto manager = reinterpret_cast<DeviceClocksManager*>(manager_ptr);
        manager->OnDriverConnected();
    }

    void DeviceClocksManager::OnRouterConnected(DDConnectionCallbacksImpl* manager_ptr, [[maybe_unused]] DDConnectionId connection_id)
    {
        const auto manager = reinterpret_cast<DeviceClocksManager*>(manager_ptr);
        manager->OnRouterConnected();
    }

    void DeviceClocksManager::OnRouterDisconnected(DDConnectionCallbacksImpl* manager_ptr)
    {
        const auto manager = reinterpret_cast<DeviceClocksManager*>(manager_ptr);
        manager->OnRouterDisconnected();
    }

    DeviceClocksManager::DeviceClocksManager(const std::shared_ptr<SystemInfoCache>& sys_info_cache,
                                             const std::shared_ptr<OverlayManager>&  overlay_manager,
                                             DDConnectionApi*                        connection_api,
                                             DDClocksApi*                            clocks_api,
                                             std::shared_ptr<Logger>                 logger)
        : sys_info_cache_(sys_info_cache)
        , overlay_manager_(overlay_manager)
        , connection_api_(connection_api)
        , clocks_api_(clocks_api)
        , logger_(logger->WithSource("Device Clocks Manager"))
    {
    }

    DeviceClocksManager::~DeviceClocksManager()
    {
        Unregister();
    }

    void DeviceClocksManager::Unregister()
    {
        if (connection_api_ != nullptr && callbacks_registered_)
        {
            connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
            callbacks_registered_ = false;
        }
    }

    bool DeviceClocksManager::Initialize()
    {
        Unregister();

        DDConnectionCallbacks connection_callbacks{};
        connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        connection_callbacks.OnDriverConnected    = &DeviceClocksManager::OnDriverConnected;
        connection_callbacks.OnDriverDisconnected = nullptr;
        connection_callbacks.OnDriverStateChanged = nullptr;
        connection_callbacks.OnRouterConnected    = &DeviceClocksManager::OnRouterConnected;
        connection_callbacks.OnRouterDisconnected = &DeviceClocksManager::OnRouterDisconnected;

        callbacks_registered_ = connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks) == DD_RESULT_SUCCESS;
        return callbacks_registered_;
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void DeviceClocksManager::OnDriverConnected() const
    {
        UpdateOverlay();
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void DeviceClocksManager::OnRouterConnected()
    {
        std::lock_guard lock(gpus_mutex_);

        const auto result = sys_info_cache_->GetSystemInfo();
        if (!result.has_value())
            return;

        const system_info_utils::SystemInfo& sys_info = result.value();

        // If there are multiple GPUs with the same name, we'll put the index in the name to make it clear which is which
        std::unordered_map<std::string, uint16_t> gpu_counts;
        std::unordered_map<std::string, uint16_t> current_gpu_index;

        for (const auto& gpu_info : sys_info.gpus)
        {
            const std::string& gpu_name = gpu_info.name;
            if (!gpu_counts.contains(gpu_info.name))
            {
                gpu_counts.insert({gpu_name, 1});
                current_gpu_index.insert({gpu_name, 0});
            }
            else
            {
                ++gpu_counts[gpu_name];
            }
        }

        updatable_gpus_.clear();

        std::vector<std::shared_ptr<DeviceClockGpu>> gpus;
        for (const auto& gpu_info : sys_info.gpus)
        {
            const std::string gpu_name     = gpu_info.name;
            const std::string gpu_name_idx = gpu_counts[gpu_name] > 1 ? fmt::format("{} [{}]", gpu_name, current_gpu_index[gpu_name]++) : gpu_name;

            updatable_gpus_.push_back(std::make_shared<UpdatableDeviceClockGpu>(gpu_name_idx, gpu_info, clocks_api_, logger_));
            gpus.push_back(updatable_gpus_.back());
        }

        gpu_info_ = {true, std::move(gpus)};

        for (auto& [listener, callback] : events_)
        {
            callback(listener, gpu_info_);
        }
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void DeviceClocksManager::OnRouterDisconnected()
    {
        std::lock_guard lock(gpus_mutex_);

        updatable_gpus_.clear();

        gpu_info_ = {false, {}};

        for (auto& [listener, callback] : events_)
        {
            callback(listener, gpu_info_);
        }
    }

    void DeviceClocksManager::RegisterEvent(const DeviceClockGpuEvent& event)
    {
        events_.push_back(event);
    }

    ClockModeType DeviceClocksManager::QueryClockMode(const uint32_t gpu_id)
    {
        std::lock_guard lock(gpus_mutex_);
        for (const auto& gpu : updatable_gpus_)
        {
            if (gpu->GetId() == gpu_id)
            {
                return gpu->QueryClockMode();
            }
        }

        return ClockModeType::kUnknown;
    }

    bool DeviceClocksManager::RequestMode(const uint32_t gpu_id, const ClockModeType mode)
    {
        bool result = false;

        std::lock_guard lock(gpus_mutex_);
        if (!force_peak_umd_connection_ids_.empty())
        {
            return result;
        }

        for (const auto& gpu : updatable_gpus_)
        {
            if (gpu->GetId() == gpu_id)
            {
                result = gpu->RequestMode(mode);
                if (result)
                {
                    UpdateOverlay();
                }
                break;
            }
        }

        return result;
    }

    void DeviceClocksManager::UpdateOverlay() const
    {
        std::map<std::string, std::string> modes;
        for (const auto& gpu : updatable_gpus_)
        {
            modes.insert({gpu->GetName(), GetClockModeTypeName(gpu->QueryClockMode())});
        }

        overlay_manager_->UpdateDeviceClocks(modes);
    }

    bool DeviceClocksManager::SetForcePeak(const DDConnectionId umd_connection_id, const bool force_peak)
    {
        std::lock_guard lock(gpus_mutex_);
        const bool      was_forcing_peak = !force_peak_umd_connection_ids_.empty();

        if (!force_peak)
        {
            force_peak_umd_connection_ids_.erase(umd_connection_id);
        }
        else
        {
            force_peak_umd_connection_ids_.insert(umd_connection_id);
        }

        const bool now_force_peak = !force_peak_umd_connection_ids_.empty();
        if (now_force_peak == was_forcing_peak)
        {
            return true;
        }

        // This isn't an atomic operation so some rollback might be needed, but failure shouldn't happen often and is mostly benign.
        const bool result = std::ranges::all_of(updatable_gpus_, [now_force_peak](const auto& gpu) { return gpu->SetForcePeak(now_force_peak); });
        UpdateOverlay();
        return result;
    }

}  // namespace devtrace
