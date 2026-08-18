// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the RGP trace source client.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_RGP_TRACE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_RGP_RGP_TRACE_CLIENT_H_

#include <algorithm>
#include <atomic>
#include <thread>

#include "asic_info.h"
#include "rgp_trace_source.h"

namespace devtrace
{
    static constexpr uint32_t kDefaultSpmSamplingFreq = 4096;    ///< The default SPM sampling frequency.
    static constexpr uint32_t kSpmMemoryLimit         = 128;     ///< The The memory limit for SPM.
    static constexpr uint32_t kNumPreparationFrames   = 4;       ///< The number of prep frames to use.
    static constexpr uint32_t kKrackan2GPUId          = 0x1902;  ///< The GPU ID for the Krackan2 GPU.

    /// @brief Private configuration used for RGP clients.
    struct RgpTraceSourceConfigPrivate
    {
        RgpTraceSourceConfig config;  ///< The public RGP trace source config.

        std::atomic_bool                        inst_tracing_supported = true;   ///< Is instruction tracing supported.
        std::atomic_bool                        ubertrace_supported    = false;  ///< Is ubertrace supported.
        std::vector<system_info_utils::GpuInfo> spm_supported_gpus;              ///< The GPUs that SPM is supported on.
    };

    /// @brief Returns whether a GPU supports SPM.
    /// @param [in] supported_gpus The list of GPUs that support SPM.
    /// @param [in] gpu The GPU to check for SPM support.
    /// @return true if the GPU supports SPM, false otherwise.,
    inline bool DoesGpuSupportsSpm(const std::vector<system_info_utils::GpuInfo>& supported_gpus, const system_info_utils::GpuInfo& gpu)
    {
        const auto find = std::ranges::find_if(supported_gpus, [&](const auto& candidate) { return candidate.name == gpu.name; });
        return find != supported_gpus.end();
    }

    static constexpr uint32_t kAllMask                  = 0xFFFFFFFF;  ///< The instruction tracing mask for all SEs.
    static constexpr uint32_t kDefaultInstTracingSeMask = 1;           ///< The default instruction tracing mask.

    /// @brief Gets the instruction tracing mask.
    /// @param [in] cu_mask The CU mask for the current GPU.
    /// @return The instruction tracing mask.
    inline uint32_t GetSeMaskForInstructionTracing(const std::vector<std::vector<uint32_t>>& cu_mask)
    {
        if (const char* inst_trace_se_mask_env_var_value_str = getenv("RDP_INST_TRACE_SE_MASK"); nullptr != inst_trace_se_mask_env_var_value_str)
        {
            // all should be the equivalent of zero, but leaving this to test if 0xFFFFFFFF behaves differently than zero
            if (strcmp(inst_trace_se_mask_env_var_value_str, "all") == 0)
            {
                return kAllMask;
            }

            if (const auto inst_trace_se_mask_env_var_value = strtol(inst_trace_se_mask_env_var_value_str, nullptr, 10);
                errno != ERANGE && inst_trace_se_mask_env_var_value >= 0)
            {
                return static_cast<uint32_t>(inst_trace_se_mask_env_var_value);
            }

            // turn off SE mask (i.e. include all SEs) in case of error
            return 0;
        }

        for (uint32_t shader_engine = 0; shader_engine < static_cast<uint32_t>(cu_mask.size()); ++shader_engine)
        {
            for (const uint32_t array_mask : cu_mask[shader_engine])
            {
                if (array_mask != 0)
                {
                    return 1 << shader_engine;
                }
            }
        }

        return kDefaultInstTracingSeMask;
    }

    /// @brief The minimum number of CUs that a GPU must have to be considered supported.
    static constexpr uint32_t kMinNumGpuCus = 3;

    /// @brief Returns true if the GPU is supported, false otherwise.
    /// @param [in] gpu The GPU to check for support.
    /// @return true if the GPU is supported, false otherwise.
    inline bool IsGpuSupportedForRgp(const system_info_utils::GpuInfo& gpu)
    {
        if (getenv("RDP_DISABLE_HARDWARE_COMPATIBILITY_CHECK") != nullptr)
        {
            return true;
        }

        const uint32_t  device_id       = gpu.asic.id_info.device;
        const uint32_t  asic_family     = gpu.asic.id_info.family;
        const uint32_t  asic_e_revision = gpu.asic.id_info.e_rev;
        const GpuSeries gpu_series      = AsicInfo::GetGpuSeries(device_id, asic_family, asic_e_revision);

        // Allow Krackan2 GPU to be supported regardless of the number of CUs.
        return (device_id == kKrackan2GPUId) || (gpu.asic.num_cus >= kMinNumGpuCus && gpu_series >= GpuSeries::kNavi1);
    }

    /// @brief Gets the true RGP capture mode for the given raw mode.
    ///
    /// If 0 is passed in, the mode will be deduced based off of the API.
    /// @param [in] mode The mode to get the RGP capture mode for.
    /// @param [in] api The API of the client.
    /// @return The RGP capture mode.
    inline RgpCaptureMode GetRgpCaptureMode(uint32_t mode, Api api)
    {
        if (mode != 0)
        {
            return static_cast<RgpCaptureMode>(mode);
        }

        switch (api)
        {
        case Api::kHip:
        case Api::kOpenCl:
            return RgpCaptureMode::kDispatch;
        default:
            break;
        }

        return RgpCaptureMode::kFrame;
    }

    /// @brief A trace that was completed with SPM.
    struct SpmTrace
    {
        std::string           path;               ///< The path on disk that the trace was output.
        SpmCounterQueryResult counters;           ///< The counters that were queried when the trace was taken.
        bool                  is_rdf = false;     ///< true if the file at the path is an RDF file.
        DDConnectionId        umd_connection_id;  ///< The id of the connection that generated the trace.
    };

    struct ISpmTraceClient
    {
        struct SpmTraceEvent
        {
            void* listener;
            void (*on_trace_completed)(void* listener, const SpmTrace& trace);
        };

        virtual ~ISpmTraceClient()                                        = default;
        virtual void RegisterSpmTraceListener(const SpmTraceEvent& event) = 0;
    };

    using RgpClient = TriggerableClient<RgpTraceSourceConfigPrivate>;

}  // namespace devtrace

#endif
