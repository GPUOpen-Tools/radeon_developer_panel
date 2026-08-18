// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the RGP trace source client.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_RGP_LEGACY_TRACE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_RGP_RGP_LEGACY_TRACE_CLIENT_H_

#include <memory>
#include <thread>

#include "../base_trace_source/trace_source_client.h"
#include "../cancellable_timer.h"

#include "active_gpu_provider.h"
#include "chunk_writing.h"

#include "dev_trace_common.h"
#include "rgp_trace_client.h"
#include "rgp_trace_source.h"

namespace devtrace
{
    struct SpmTraceCompletionEventArgs
    {
        SpmTrace spm_trace;  ///< The SPM trace that was completed.
    };

    struct SpmTraceCompletionEvent
    {
        void*                                                          object = nullptr;  ///< The object to call the callback on.
        std::function<void(void*, const SpmTraceCompletionEventArgs&)> callback;          ///< The callback to call when an SPM trace completes.
    };

    /// @brief RGP legacy client.
    class RgpLegacyClient final : public RgpClient, public ISpmTraceClient
    {
    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @param [in] profiling_api The memory trace API.
        /// @param [in] counter_handler Utility for handling counters.
        /// @param [in] active_gpu_provider Provider for active GPUs.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        RgpLegacyClient(ClientConnection                              conn_info,
                        ClientUtils<RgpTraceSourceConfigPrivate>&     client_utils,
                        DDGpuProfilingApi*                            profiling_api,
                        const std::shared_ptr<RgpSpmCounterHandler>&  counter_handler,
                        std::unique_ptr<ActiveGpuProvider>&           active_gpu_provider,
                        const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer);

        ~RgpLegacyClient() override;

        DD_DRIVER_STATE GetInitDriverState() override;

        Result Initialize() override;

        void Disconnect() override;

        Result HandleDriverState(DD_DRIVER_STATE state) override;

        static void OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args);

        void RegisterSpmTraceListener(const SpmTraceEvent& event) override;

    private:
        /// @brief Requests that a trace be taken but will emit trace failed if it fails.
        ///
        /// This function exits to ensure that the config doesn't change between starting an auto capture and checking the params again.
        /// @param [in] config The client config.
        /// @param [in] use_frame_capture true if frame capture should be used, false otherwise.
        /// @param [in] auto_capture_mode The auto capture mode.
        /// @return kSuccess if the trace was successfully requested.
        Result RequestAutoCapture(const RgpTraceSourceConfigPrivate& config, bool use_frame_capture, AutoCaptureMode auto_capture_mode);

        /// @brief Initiates a timer based capture.
        /// @param [in] config The config to use.
        /// @return The result of beginning the timer based capture.
        Result RunTimerCapture(const RgpTraceSourceConfigPrivate& config);

        /// @brief Callback handles updating the active GPU from polling
        /// @param [in] self A pointer to this client.
        /// @param [in] gpu The active GPU discovered through polling system info.
        static void OnActiveGpuUpdate(void* self, const system_info_utils::GpuInfo& gpu);

    public:
        Result RequestAbortTrace() override;
        bool   IsAbortTraceSupported() override;

        Result             PrepareForDelayedCapture() override;
        Result             RequestBeginTrace(uint32_t capture_mode) override;
        [[nodiscard]] bool SupportsCaptureMode(uint32_t mode) const override;

    private:
        /// @brief Requests that a trace be taken.
        ///
        /// This function exits to ensure that the config doesn't change between starting an auto capture and checking the params again.
        /// @param [in] capture_mode The mode to capture with.
        /// @param [in] config The client config.
        /// @param [in] use_frame_capture true if frame capture should be used, false otherwise.
        /// @param [in] auto_capture_mode The auto capture mode.
        /// @return kSuccess if the trace was successfully requested.
        Result RequestBeginTrace(RgpCaptureMode                     capture_mode,
                                 const RgpTraceSourceConfigPrivate& config,
                                 bool                               use_frame_capture,
                                 AutoCaptureMode                    auto_capture_mode);

        /// @brief Queries the SPM counters and then updates them.
        /// @param [in] config The config for the trace.
        /// @param [out] gpu The GPU to use for the counter query.
        /// @param [out] query_result The result of the SPM query.
        Result UpdateSpmCounters(const RgpTraceSourceConfig& config, const system_info_utils::GpuInfo& gpu, SpmCounterQueryResult& query_result);

        /// @brief Executes a trace.
        /// @param [in] config The config for the trace.
        /// @param [in] spm_counters The SPM counters to use for the trace.
        /// @param [in] timeout The timeout to use for the capture.
        /// @param [in] is_auto_capture true if the capture was automatically taken.
        void ExecuteTrace(const DDGpuProfilingConfig& config, const SpmCounterQueryResult& spm_counters, uint32_t timeout, bool is_auto_capture);

        /// @brief Finalizes a trace based off of the result.
        /// @param [in] result The result of execute trace.
        /// @param [in] additional_chunk_result The result of writing the additional chunks.
        /// @param [in] path The path of the trace.
        /// @param [in] config The config for the trace.
        /// @param [in] spm_counters The SPM counters to use for the trace.
        /// @param [in] is_auto_capture true if the capture was automatically taken.
        /// @param [in] was_aborted true if the capture was aborted, false otherwise.
        void FinalizeTrace(DD_RESULT                    result,
                           Result                       additional_chunk_result,
                           const std::string&           path,
                           const DDGpuProfilingConfig&  config,
                           const SpmCounterQueryResult& spm_counters,
                           bool                         is_auto_capture,
                           bool                         was_aborted);

        /// @brief Called when DevDriver begins a trace.
        /// @param [in] userdata The client that began a trace.
        static void OnBeginTrace(void* userdata);

        /// @brief Generates the DevDriver configuration for profiling from the configuration.
        /// @param [in] capture_mode The mode to capture with.
        /// @param [in] config The client config.
        /// @param [in] inst_tracing_mask The instruction tracing mask.
        /// @param [in] spm_supported true if SPM is supported, false otherwise.
        /// @param [in] use_frame_capture true if frame capture should be used, false otherwise.
        /// @param [in] auto_capture_mode The auto capture mode.
        /// @param [out] rgp_config The generated RGP config.
        static void GenerateDdGpuProfilingConfig(RgpCaptureMode                     capture_mode,
                                                 const RgpTraceSourceConfigPrivate& config,
                                                 uint32_t                           inst_tracing_mask,
                                                 bool                               spm_supported,
                                                 bool                               use_frame_capture,
                                                 AutoCaptureMode                    auto_capture_mode,
                                                 DDGpuProfilingConfig&              rgp_config);

        /// @brief Gets the capture timeout.
        /// @param [in] capture_mode The mode to capture with.
        /// @param [in] config The config options being used for the capture.
        /// @param [in] rgp_config The generated RGP config.
        /// @param [in] use_frame_capture true if frame capture should be used, false otherwise.
        /// @param [in] auto_capture_mode The auto capture mode.
        /// @return The timeout in milliseconds for a capture.
        static uint32_t GetTimeout(RgpCaptureMode              capture_mode,
                                   const RgpTraceSourceConfig& config,
                                   const DDGpuProfilingConfig& rgp_config,
                                   bool                        use_frame_capture,
                                   AutoCaptureMode             auto_capture_mode);

        /// @brief Returns whether this client is ready to perform a capture.
        /// @return true if this client is ready for a capture, false otherwise.
        [[nodiscard]] bool IsReadyForCapture();

        ClientConnection                          conn_info_{};                                           ///< The connection info for the client.
        ClientUtils<RgpTraceSourceConfigPrivate>& client_utils_;                                          ///< The utils that the client can use.
        DDGpuProfilingApi*                        profiling_api_ = nullptr;                               ///< The profiling API.
        std::shared_ptr<RgpSpmCounterHandler>     counter_handler_;                                       ///< Utility for handling counters.
        std::unique_ptr<ActiveGpuProvider>        active_gpu_provider_;                                   ///< The provider of the active GPU.
        std::shared_ptr<AdditionalChunkWriter>    additional_chunk_writer_;                               ///< The object that does additional chunk writing.
        system_info_utils::GpuInfo                active_gpu_;                                            ///< The active GPU.
        ChunkWriterReservationId                  chunk_reservation_ = kInvalidChunkWriterReservationId;  ///< Reservation for chunk writer.
        std::thread                               capture_thread_;                                        ///< The thread that capturing is performed on.
        std::mutex                                abort_mutex_;                                           ///< Mutex that guards abort.
        std::atomic_bool                          aborted_ = false;                                       ///< Whether the trace has been aborted.
        CancellableTimer                          timer_;                                                 ///< Timer for compute auto capture.
        SpmTraceEvent                             spm_trace_event_{};                                     ///< Event for when an SPM trace completes.
    };

}  // namespace devtrace

#endif
