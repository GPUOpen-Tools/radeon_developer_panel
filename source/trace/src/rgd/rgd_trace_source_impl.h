// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for concrete implementation of the RGD trace source.

#ifndef RDP_SOURCE_TRACE_SRC_RGD_TRACE_SOURCE_IMPL_H_
#define RDP_SOURCE_TRACE_SRC_RGD_TRACE_SOURCE_IMPL_H_

#include <atomic>
#include <memory>
#include <string>

#include <ddApi.h>
#include <dd_enhanced_crash_info_api.h>
#include <dd_gpu_detective_api.h>

#include <system_info_reader.h>

#include "../base_trace_source/base_continuous_trace_source.h"
#include "../base_trace_source/trace_source_client.h"
#include "chunk_writing.h"
#include "configurable.h"
#include "dd_uber_trace_api.h"

#include "dev_trace_common.h"
#include "dipper.h"
#include "logging.h"
#include "rgd_client.h"
#include "rgd_summary_generator.h"
#include "rgd_trace_source.h"
#include "trace_io.h"
#include "ubertrace_factory.h"

namespace devtrace
{
    /// @brief Factory for RGD clients.
    class RgdClientFactory final : public ClientFactory<RgdClient>
    {
    public:
        /// @brief Constructor.
        /// @param [in] gpu_detective_api The RGD API.
        /// @param [in] ubertrace_api The UberTrace API.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] enhanced_api The API for enabling enhanced crash analysis.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        RgdClientFactory(DDGpuDetectiveApi*                            gpu_detective_api,
                         DDUberTraceApi*                               ubertrace_api,
                         const std::shared_ptr<UbertraceUserFactory>&  ubertrace_factory,
                         DDEnhancedCrashInfoApi*                       enhanced_api,
                         const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer);

        ~RgdClientFactory() override;

        std::unique_ptr<RgdClient> CreateClient(const ClientConnection& info, ClientUtils<RgdTraceSourceConfig>& client_utils) override;

        /// @brief Sets whether or not UberTrace is enabled.
        /// @param [in] enabled true if it should be enabled, false otherwise.
        void SetUbertraceEnabled(bool enabled);

        /// @brief Sets the UberTrace features.
        /// @param [in] features The UberTrace driver features.
        void SetUbertraceFeatures(const UbertraceFeatures& features) const;

    private:
        DDGpuDetectiveApi*                     gpu_detective_api_ = nullptr;  ///< The RGD API.
        DDUberTraceApi*                        ubertrace_api_     = nullptr;  ///< The UberTrace API.
        std::shared_ptr<UbertraceUserFactory>  ubertrace_factory_;            ///<  The UberTrace user factory.
        DDEnhancedCrashInfoApi*                enhanced_api_ = nullptr;       ///< The API for enabling enhanced crash analysis.
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer_;      ///< The object that does additional chunk writing.
        std::atomic_bool                       enable_ubertrace_ = false;     ///< true if UberTrace should be enabled, false otherwise.
    };

    /// @brief Concrete implementation of the RGD trace source.
    class RgdTraceSourceImpl final : public RgdTraceSource
    {
        /// @brief Determines if RGD is supported on the system with the given information, and if not returns the reason it should be disabled.
        /// @param [in] system_info The system information to process.
        /// @return DisabledReason::kEnabled if RGD is supported, otherwise the reason it should be disabled.
        static DisabledReason GetReasonRgdShouldBeDisabled(const system_info_utils::SystemInfo& system_info);

    public:
        /// @brief Callback for completed traces.
        /// @param [in] userdata The user data pointer.
        /// @param [in] args The event arguments.
        /// This function will queue summary generation if configured to do so.
        static void OnTraceCompleted(void* userdata, const TraceCompletionEventArgs& args);

        /// @brief Constructor.
        /// @param [in] stream_provider An object used to acquire read / write streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] summary_generator An object that can be used to generate crash summaries.
        /// @param [in] system_info_cache A cache for the system information.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        /// @param [in] gpu_detective_api The crash analysis API.
        /// @param [in] ubertrace_api The UberTrace API.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] enhanced_api The enhanced crash info API.
        /// @param [in] logger An object that can be used for logging.
        DIP(RgdTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>&     stream_provider,
                               const std::shared_ptr<OverlayManager>&              overlay_manager,
                               const std::shared_ptr<RgdSummaryGenerator>&         summary_generator,
                               const std::shared_ptr<class SystemInfoCache>&       system_info_cache,
                               const std::shared_ptr<class AdditionalChunkWriter>& additional_chunk_writer,
                               DDGpuDetectiveApi*                                  gpu_detective_api,
                               DDUberTraceApi*                                     ubertrace_api,
                               const std::shared_ptr<UbertraceUserFactory>&        ubertrace_factory,
                               DDEnhancedCrashInfoApi*                             enhanced_api,
                               const std::shared_ptr<Logger>&                      logger));

        /// @brief Destructor.
        ~RgdTraceSourceImpl() override = default;

        void RouterConnectionStatusChanged(bool is_connected) override;
        void OnDriverConnected(const DDConnectionInfo& connection_info) override;
        void OnDriverDisconnected(DDConnectionId umd_connection_id) override;
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) override;

        Result RequestAbortTrace(DDConnectionId umd_connection_id) override;
        Result RequestAbortProcessing() override;

        void RegisterStatusEvent(const TraceSourceStatusEvent& event) override;

        void RegisterTraceCompletionEvent(const TraceCompletionEvent& event) override;

        void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event) override;

        void RegisterSupportEvent(const RgdTraceSourceSupportEvent& event) override;

        void RegisterSummaryEvent(const RgpSummaryEvent& event) override;

        void QueryStatus() override;

        Result RequestDump(DDConnectionId connection_id) override;
        Result AddMarker(DDConnectionId connection_id, const std::string& marker) override;

        RgdTraceSourceConfig& GetConfig() override;

        Result QueueSummaryGeneration(const std::string& path, bool generate_text, bool generate_json) override;

        /// @brief Registers no crash event callback
        /// @param [in] event Struct that defines listener and callback function.
        void RegisterNoCrashDetectedEvent(const RgdNoCrashDetectedEvent& event) override;

        /// @brief Posts the no crash detected event to registered listeners.
        void PostNoCrashDetectedEvent() const;

        /// @brief Returns whether the current hardware is an APU device.
        /// @return true if current hardware is an APU device, false otherwise.
        bool IsCurrentHardwareApu() const override;

        /// @brief Returns whether the current device supports hardware crash analysis.
        /// @return true if current hardware is a supported hardware crash analysis device, false otherwise.
        bool IsHardwareCrashAnalysisSupported() const override;

    private:
        void PostSummaryEvent(const RgdSummaryEventArgs& args) const;

        void PostSupportEvent(const RgdTraceSourceSupportEventArgs& args) const;

        /// @brief Queues the generation of an RGD summary.
        /// @param [in] path The path of the RGD file to generate the summary for (UTF-8 encoded)
        /// @param [in] generate_text true if the text summary should be generated, false otherwise.
        /// @param [in] generate_json true if the json summary should be generated, false otherwise.
        /// @param [in] automatic true if the summary was queued automatically in response to a dump.
        /// @return Result::kSuccess if summary queuing was successful.
        Result QueueSummaryGenerationInternal(const std::string& path, bool generate_text, bool generate_json, bool automatic);

        const std::shared_ptr<SystemInfoCache> system_info_cache_;              ///< Object used to query the system info.
        std::shared_ptr<RgdSummaryGenerator>   summary_generator_;              ///< Object used to generate summaries.
        std::shared_ptr<Logger>                logger_;                         ///< Object used for debug logging.
        std::shared_ptr<RgdClientFactory>      factory_;                        ///< The factory used to create clients.
        BaseContinuousTraceSource<RgdClient>   base_trace_source_;              ///< Base trace source.
        std::atomic_bool                       should_abort_summary_ = false;   ///< true if the summary generation should be aborted, false otherwise.
        std::mutex                             support_event_mutex_;            ///< Mutex guards support event.
        RgdTraceSourceSupportEvent             support_event_;                  ///< Support event callback.
        mutable std::mutex                     no_crash_detected_event_mutex_;  ///< Mutex guards no crash detected event.
        RgdNoCrashDetectedEvent                no_crash_detected_event_;        ///< No crash detected event callback.
        std::atomic_bool                       current_hardware_is_apu_           = false;  ///< true if current hardware is an APU.
        std::atomic_bool                       hardware_crash_analysis_supported_ = false;  ///< true if hardware crash analysis is supported.
        std::mutex                             summary_event_mutex_;                        ///< Mutex guards summary event.
        RgpSummaryEvent                        summary_event_;                              ///< Summary event callback.
    };

}  // namespace devtrace

#endif
