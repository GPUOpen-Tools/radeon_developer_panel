// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for object that provides the active GPU.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_ACTIVE_GPU_PROVIDER_H_
#define RDP_SOURCE_TRACE_SRC_RGP_ACTIVE_GPU_PROVIDER_H_

#include <mutex>
#include <semaphore>
#include <thread>

#include <dd_common_api.h>

#include <system_info_reader.h>
#include <system_info_writer.h>

struct DDDriverUtilsApi;

namespace devtrace
{
    struct ActiveGpuEvent
    {
        void*                                                         listener;
        std::function<void(void*, const system_info_utils::GpuInfo&)> callback;
    };

    struct SystemGpuInfo
    {
        std::vector<system_info_utils::GpuInfo> gpus;
        std::vector<system_info_utils::GpuInfo> spm_supported_gpus;
    };

    /// @brief Object that provides the active GPU.
    class ActiveGpuProvider final
    {
    public:
        /// @brief Constructor.
        /// @param [in] driver_utils_api The driver utils API from devdriver registry.
        /// @param [in] system_gpu_info The GPUs available in the system.
        /// @param [in] umd_connection_id The UMD connection id to poll.
        explicit ActiveGpuProvider(DDDriverUtilsApi* driver_utils_api, const SystemGpuInfo& system_gpu_info, DDConnectionId umd_connection_id);

        /// @brief  Starts polling for active GPU on dedicated thread.
        /// @param [in] event The event struct to trigger update for when active GPU is found.
        void StartPolling(ActiveGpuEvent event);

        /// @brief Stops polling for active GPU.
        void StopPolling();

        /// @brief Gets the active GPU found through polling system info
        /// @return The active GPU found through polling system info
        system_info_utils::GpuInfo GetActiveGpu();

        /// @brief Gets if the specified GPU is within the list of SPM supported gpus.
        /// @return true if SPM is supported for this GPU, false otherwise.
        bool DoesGpuSupportSpm(const system_info_utils::GpuInfo& gpu) const;

        /// @brief Queries the active GPU.
        /// @return The active GPU.
        system_info_utils::GpuInfo QueryActiveGpu();

    private:
        DDDriverUtilsApi*                       driver_utils_api_ = nullptr;  ///< The driver utils API from devdriver registry.
        std::vector<system_info_utils::GpuInfo> gpus_;                        ///< The GPUs currently in the system.
        std::vector<system_info_utils::GpuInfo> spm_supported_gpus_;          ///< The GPUs in the system with SPM capture support.
        DDConnectionId                          umd_connection_id_;           ///< The UMD connection id to poll.
        std::thread                             thread_;                      ///< Polling thread
        std::binary_semaphore                   exit_thread_semaphore_{0};    /// Semaphore to signal polling thread to exit.
        std::mutex                              active_gpu_mutex_;            ///< Active gpu mutex
        system_info_utils::GpuInfo              active_gpu_;                  ///< Active gpu
    };
}  // namespace devtrace

#endif
