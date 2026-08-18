// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base RGP trace source.

#ifndef RDP_SOURCE_TRACE_INC_RGP_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_RGP_TRACE_SOURCE_H_

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <dd_gpu_profiling_api.h>

#include "configurable.h"
#include "counters/rgp_spm_counter_handler.h"

#include "source_userdata.h"
#include "system_info_reader.h"
#include "trace_source.h"
#include "triggerable_trace_source.h"

namespace devtrace
{
    struct RgpTraceSourceSupportEventArgs
    {
        bool is_instruction_tracing_supported                = true;
        bool is_spm_capture_supported                        = true;
        bool is_shader_instrumentation_supported_universally = true;
        bool is_exec_pop_tokens_supported                    = false;  ///< true if exec/pop count tokens are supported (RDNA4+).
    };

    struct RgpTraceSourceSupportEvent
    {
        void*                                                             listener;
        std::function<void(void*, const RgpTraceSourceSupportEventArgs&)> callback;
    };

    /// @brief The different capture modes for RGP.
    enum class RgpCaptureMode : uint32_t
    {
        kDefault = 0,
        kFrame,
        kDraw,
        kDispatch,
        kCount
    };

    /// @brief Configuration for RRA trace source.
    struct RgpTraceSourceConfig
    {
        /// @brief The SQTT buffer size of one SE for each profile in MB.
        ///
        /// The size of this array should be the same as the length of kSQTTProfileNames. To get the size of
        /// an SE's SQTT buffer for the profile kSQTTProfileNames[x], use kSQTTProfileSizes[x].
        ///
        /// If the driver supports per-SE instruction tracing, the driver will apply a 4x multiplier to the SE where instruction tracing is enabled to use a larger buffer for those SEs.
        /// If the driver does not support per-SE instruction tracing, the values will remain unmultiplied.
        /// The values in this array correspond to the base (non-instruction traced) SE buffer size.
        static constexpr std::array<uint32_t, 5> kSqttProfileSizes = {16, 32, 75, 128, 256};

        std::atomic_bool             disable_capture_timeout      = false;                 ///< Disables capture timeout.
        std::atomic_bool             enable_spm_counters          = true;                  ///< true if SPM counters should be collected.
        std::atomic_bool             enable_inst_tracing          = false;                 ///< true if instruction tracing should be collected.
        std::atomic_bool             enable_legacy_capture        = false;                 ///< true if Legacy capture should be enabled, false otherwise.
        std::atomic<AutoCaptureMode> auto_capture_mode            = kAutoCaptureModeNone;  ///< Compute auto capture mode
        std::atomic_uint32_t         compute_auto_capture_time_ms = 0;                     ///< The time delay for compute based auto capture.
        std::atomic<uint32_t>        dispatch_start_index         = 1;                     ///< The start index for dispatch captures.
        std::atomic_uint32_t         dispatch_count               = 0;                     ///< Dispatch count.
        std::atomic_uint32_t         draw_count                   = 1;                     ///< Draw count.
        std::atomic_uint32_t         sqtt_memory_limit            = 0;                     ///< SQTT memory limit;
        std::atomic<uint32_t>        frame_capture_index          = 0;                     ///< The frame index to capture.
        std::atomic_bool             enable_single_token_sqtt     = false;                 ///< true if single token SQTT should be enabled, false otherwise.
        std::atomic_bool             enable_exec_pop_tokens       = false;                 ///< true if Exec/Pop count SQTT tokens should be collected.
    };

    /// @brief The result of SPM derived counter processing.
    struct SpmProcessingResult
    {
        std::string path;    ///< The path of the file that was processed.
        Result      result;  ///< The result of processing.
    };

    /// @brief Abstract RGP trace source.
    class RgpTraceSource : public TriggerableTraceSource, public Configurable<RgpTraceSourceConfig>
    {
    public:
        /// @brief Destructor.
        ~RgpTraceSource() override = default;

        virtual void RegisterSupportEvent(const RgpTraceSourceSupportEvent& event) = 0;

        /// @brief Sets whether shader instrumentation is enabled.
        /// @param [in] enabled true if shader instrumentation should be enabled, false otherwise.
        /// @return The result of enabling or disabling shader instrumentation.
        virtual Result SetShaderInstrumentationEnabled(bool enabled) = 0;
    };

};  // namespace devtrace

#endif
