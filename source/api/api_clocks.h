// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for clock mode handler for the capture API.

#ifndef SOURCE_API_CAPTURE_API_CLOCKS_H_
#define SOURCE_API_CAPTURE_API_CLOCKS_H_

#include <vector>

#include <dd_clocks_api.h>

#include <dipper.h>

#include "RdpCaptureApi.h"

namespace devtrace
{
    class DeviceClocksManager;
    class DeviceClockGpu;
    struct DeviceClockGpuInfo;
}  // namespace devtrace

namespace system_info_utils
{
    struct system_info;
}

/// @brief Details about a GPU needed for device clocks.
struct ClockGpuDetails
{
    uint32_t                                   gpu_id;     ///< Identifier of the GPU.
    std::vector<RdpCaptureGpuClockModeDetails> api_modes;  ///< The clock modes.
};

/// @brief Object that handles device clocks.
class ApiClocks
{
public:
    /// @brief Constructor.
    /// @param [in] device_clocks_manager The device clock manager.
    DIP(ApiClocks(const std::shared_ptr<devtrace::DeviceClocksManager>& device_clocks_manager));

    static void OnGpuEvent(void* object, const devtrace::DeviceClockGpuInfo& gpu_info);

private:
    /// @brief Converts the GPUs from devtrace.
    void ConvertGpus(const devtrace::DeviceClockGpuInfo& gpu_info);

public:
    /// @brief Gets the details of the supported clock modes for the given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to get the supported clock modes for.
    /// @param [out] modes The supported clock modes for the GPU (should be freed with RdpCaptureFnFree()).
    /// @param [out] num_modes The number of modes in the @param modes array.
    void GetGpuClockModes(uint64_t gpu_index, RdpCaptureGpuClockModeDetails** modes, uint64_t* num_modes) const;

    /// @brief Queries the current clock mode of a given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to query the clock mode for.
    /// @param [out] mode The current clock mode for the GPU.
    /// @return The result of querying the current clock mode for the GPU.
    RdpCaptureResult QueryGpuCurrentClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode* mode) const;

    /// @brief Sets the current clock mode of a given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to set the clock mode for.
    /// @param [in] mode The desired clock mode for the GPU.
    RdpCaptureResult SetCurrentGpuClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode mode) const;

private:
    std::shared_ptr<devtrace::DeviceClocksManager> device_clocks_manager_ = nullptr;  ///< The device clock manager.
    std::vector<ClockGpuDetails>                   gpus_;  ///< The details for each GPU. Order is the same as the order in system info.
};

#endif
