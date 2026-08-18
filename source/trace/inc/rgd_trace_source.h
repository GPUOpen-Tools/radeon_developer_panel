// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base RGD trace source.

#ifndef RDP_SOURCE_TRACE_INC_RGD_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_RGD_TRACE_SOURCE_H_

#include <atomic>
#include <mutex>
#include <string>

#include "configurable.h"
#include "continuous_trace_source.h"
#include "rgd_summary_options.h"

namespace devtrace
{
    struct RgdTraceSourceSupportEventArgs
    {
        bool is_hardware_crash_analysis_supported = false;  ///< true if both the driver and GPU support Hardware Crash Analysis.
        bool is_hardware_apu                      = false;  ///< true if the current hardware is an APU.
        /// true if the driver is new enough (>= 25.20) to emit SGPR/VGPR data during crash analysis.
        /// This is a driver-version property only — it is independent of is_hardware_crash_analysis_supported.
        /// The UI enforces the conjunction: GPR options are interactive only when both flags are true.
        bool is_gpr_capture_supported = false;
    };

    struct RgdTraceSourceSupportEvent
    {
        void*                                                             listener = nullptr;
        std::function<void(void*, const RgdTraceSourceSupportEventArgs&)> callback;
    };

    /// @brief Event for when a client disconnects without a crash being detected.
    struct RgdNoCrashDetectedEvent
    {
        void*                      listener = nullptr;
        std::function<void(void*)> callback;
    };

    /// @brief Configuration for RGD trace source.
    struct RgdTraceSourceConfig
    {
        std::atomic_bool generate_text_summary             = false;  ///< true if the text summary should be created when a dump is taken.
        std::atomic_bool generate_json_summary             = false;  ///< true if the JSON summary should be created when a dump is taken.
        std::atomic_bool enable_advanced_crash             = true;   ///< true if advanced crash analysis should be enabled.
        std::atomic_bool disable_serialize_mem_ops         = false;  ///< true if memory ops should not be serialized with enhanced crash.
        std::atomic_bool disable_serialize_alu_ops         = false;  ///< true if alu ops should not be serialized with enhanced crash.
        std::atomic_bool hardware_crash_analysis_supported = false;  ///< true if hardware crash analysis is supported.
        std::atomic_bool collect_wave_sgprs                = false;  ///< true to collect wave SGPRs (scalar general-purpose registers).
        std::atomic_bool collect_wave_vgprs                = false;  ///< true to collect wave VGPRs (vector general-purpose registers).

        /// @brief Gets the current summary options.
        /// @return The current summary options.
        RgdSummaryOptions GetSummaryOptions() const
        {
            std::scoped_lock lock(summary_mutex);
            return summary_options;
        }

        /// @brief Sets the summary options.
        /// @param [in] new_options The new summary options.
        void SetSummaryOptions(const RgdSummaryOptions& new_options)
        {
            std::scoped_lock lock(summary_mutex);
            summary_options = new_options;
        }

    private:
        mutable std::mutex summary_mutex;      ///< Mutex that guards the summary options.
        RgdSummaryOptions  summary_options{};  /// < The additional options for summary generation.
    };

    /// @brief The result of a summary generation operation.
    struct SummaryResult
    {
        Result      result;                ///< The result of the generation.
        std::string path;                  ///< The path that the trace file was output to.
        bool        automatically_queued;  ///< true if the summary was automatically queued from a crash dump.
        std::string error;                 ///< The error string (if any).
    };

    struct RgdSummaryEventArgs
    {
        SummaryResult result;  ///< The result of the summary generation.
    };

    struct RgpSummaryEvent
    {
        void*                                                  listener;  ///< The listener object.
        std::function<void(void*, const RgdSummaryEventArgs&)> callback;  ///< The callback function.
    };

    /// @brief Abstract RGD trace source.
    class RgdTraceSource : public ContinuousTraceSource, public Configurable<RgdTraceSourceConfig>
    {
    public:
        /// @brief Destructor.
        ~RgdTraceSource() override = default;

        /// @brief Queues the generation of an RGD summary.
        /// @param [in] path The path of the RGD file to generate the summary for (UTF-8 encoded)
        /// @param [in] generate_text true if the text summary should be generated, false otherwise.
        /// @param [in] generate_json true if the json summary should be generated, false otherwise.
        /// @return Result::kSuccess if summary queuing was successful.
        virtual Result QueueSummaryGeneration(const std::string& path, bool generate_text, bool generate_json) = 0;

        virtual void RegisterSupportEvent(const RgdTraceSourceSupportEvent& event) = 0;

        virtual void RegisterNoCrashDetectedEvent(const RgdNoCrashDetectedEvent& event) = 0;

        virtual void RegisterSummaryEvent(const RgpSummaryEvent& event) = 0;

        /// @brief Returns whether the current hardware is an APU device.
        /// @return true if current hardware is an APU device, false otherwise.
        [[nodiscard]] virtual bool IsCurrentHardwareApu() const = 0;

        /// @brief Gets an observable that emits if hardware crash analysis is supported on the system.
        /// @return An observable that emits true if hardware crash analysis is supported, false otherwise.
        [[nodiscard]] virtual bool IsHardwareCrashAnalysisSupported() const = 0;
    };

}  // namespace devtrace

#endif
