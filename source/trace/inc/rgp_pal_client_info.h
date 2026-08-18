// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Pal Client Info utility

#ifndef RDP_SOURCE_TRACE_INC_RGP_PAL_CLIENT_INFO_H_
#define RDP_SOURCE_TRACE_INC_RGP_PAL_CLIENT_INFO_H_

#include <memory>
#include <string>
#include <vector>

#include <system_info_reader.h>

#include <dd_driver_utils_api.h>

struct TargetGpu
{
    std::string gpu_name;
    uint32_t    device_id;
    uint32_t    revision_id;
    uint32_t    vendor_id;
    bool        is_finalized;
    uint32_t    queue_count;
    uint32_t    frame_count;
    uint32_t    attached_screens;
};

class PalClientInfo
{
public:
    /// @brief Queries PAL client info
    /// @param [in] client_context The client context
    /// @return PalClientInfo object
    static std::shared_ptr<PalClientInfo> Query(DDDriverUtilsApi* driver_utils_api, DDConnectionId connection_id);

    /// @brief Destructor
    ~PalClientInfo() = default;

    /// @brief Gets the active GPU from the list of candidates.
    /// @param [in] candidates The candidates to use.
    /// @return target GPU.
    system_info_utils::GpuInfo GetActiveGpu(const std::vector<system_info_utils::GpuInfo>& candidates) const;

private:
    /// @brief Matches the target GPU with one from the candidate list.
    /// @param [in] gpu The GPU to match with the candidate list.
    /// @param [in] candidates The candidates to match against.
    /// @return The matching GPU.
    static system_info_utils::GpuInfo GetCorrespondingGpu(const TargetGpu& gpu, const std::vector<system_info_utils::GpuInfo>& candidates);

    /// @brief PAL client info query begin callback
    /// @param [in] user_data User data
    /// @param [in] data_sz Total payload size
    /// @return DD_RESULT_SUCCESS
    static DD_RESULT QueryInfoBegin(void* user_data, const size_t* data_sz);

    /// @brief PAL client info query write callback
    /// @param [in] user_data User data
    /// @param [in] data The payload
    /// @param [in] data_sz The payload size in bytes
    /// @return DD_RESULT_SUCCESS
    static DD_RESULT QueryInfoWrite(void* user_data, const void* data, size_t data_sz);

    /// @brief PAL client info query end callback
    /// @param [in] user_data User data
    /// @param result Result code from write callback status return.
    static void QueryInfoEnd(void* user_data, DD_RESULT result);

    /// @brief Scores the target GPU.
    /// @param [in] gpu The GPU to score.
    /// @return The score of the GPU.
    static uint32_t Score(const TargetGpu& gpu);

    std::vector<TargetGpu> gpus_;      ///< List of GPU reported by PAL
    std::string            json_str_;  ///< JSON string
    size_t                 data_sz_;   ///< Cache data size
};

#endif
