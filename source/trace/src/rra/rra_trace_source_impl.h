// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for concrete implementation of the RRA trace source.

#ifndef RDP_SOURCE_TRACE_SRC_RRA_RRA_TRACE_SOURCE_IMPL_H_
#define RDP_SOURCE_TRACE_SRC_RRA_RRA_TRACE_SOURCE_IMPL_H_

#include <atomic>
#include <memory>
#include <mutex>

#include <ddApi.h>
#include <dd_uber_trace_api.h>

#include <system_info_reader.h>

#include "../base_trace_source/base_triggerable_trace_source.h"
#include "../base_trace_source/ubertrace_client.h"
#include "configurable.h"
#include "dev_trace_common.h"
#include "dipper.h"
#include "logging.h"
#include "rra_trace_source.h"
#include "source_status.h"
#include "system_info_cache.h"
#include "trace_io.h"

namespace devtrace
{
    /// @brief Private configuration used for RRA clients.
    struct RraTraceSourceConfigPrivate
    {
        RraTraceSourceConfig config;                    ///< The public RRA trace source config.
        std::atomic_bool     ray_history_is_supported;  ///< Whether ray history is supported.
    };

    /// @brief Client used for RRA.
    class RraClient final : public UbertraceClient<RraTraceSourceConfigPrivate>
    {
    public:
        RraClient(const ClientConnection&                       conn_info,
                  ClientUtils<RraTraceSourceConfigPrivate>&     client_utils,
                  std::unique_ptr<UbertraceUser>                user,
                  const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer);

        ~RraClient() override;

    protected:
        void   GetPreliminarySources(std::vector<UberTraceSource>& sources) override;
        Result GenerateCaptureConfig(const RraTraceSourceConfigPrivate& config, UbertraceCaptureConfig& capture_config) override;

    public:
        [[nodiscard]] bool SupportsCaptureMode(uint32_t mode) const override;
    };

    /// @brief Factory type for RRA clients.
    class RraClientFactory final : public ClientFactory<RraClient>
    {
    public:
        /// @brief Constructor.
        /// @param [in] api The API to pass to the clients.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        RraClientFactory(DDUberTraceApi*                                    api,
                         const std::shared_ptr<class UbertraceUserFactory>& ubertrace_factory,
                         const std::shared_ptr<AdditionalChunkWriter>&      additional_chunk_writer);

        ~RraClientFactory() override;

        /// @brief Creates a new client.
        /// @param [in] info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @return The new client.
        std::unique_ptr<RraClient> CreateClient(const ClientConnection& info, ClientUtils<RraTraceSourceConfigPrivate>& client_utils) override;

        /// @brief Sets the UberTrace driver features.
        /// @param [in] features The UberTrace driver features.
        void SetFeatures(const UbertraceFeatures& features) const;

    private:
        DDUberTraceApi*                        api_ = nullptr;            ///< The API to pass to the clients.
        std::shared_ptr<UbertraceUserFactory>  ubertrace_factory_;        ////< The UberTrace user factory.
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer_;  ///< Object used for system info writing.
    };

    /// @brief Abstract trace source.
    class RraTraceSourceImpl final : public RraTraceSource
    {
        /// @brief Determines if RRA is supported on the system with the given information, and if not returns the reason it should be disabled.
        /// @param [in] system_info The system information to process.
        /// @return DisabledReason::kEnabled if RRA is supported, otherwise the reason it should be disabled.
        static DisabledReason GetReasonRraShouldBeDisabled(const system_info_utils::SystemInfo& system_info);

        /// @brief Determines if ray history feature is supported on the system
        /// @param [in] system_info The system information to process.
        /// @return true if ray history supported, false otherwise.
        static bool IsRayHistorySupported(const system_info_utils::SystemInfo& system_info);

        /// @brief Determines if marker-based capture is supported on the system
        /// @param [in] system_info The system information to process.
        /// @return true if marker-based capture is supported, false otherwise.
        static bool IsMarkerCaptureSupported(const system_info_utils::SystemInfo& system_info);

    public:
        /// @brief Constructor.
        /// @param [in] stream_provider An object used to acquire writable streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache A cache for the system information.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] uber_trace_api The ubertrace API.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] logger Object used for debug logging.
        DIP(RraTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>&    stream_provider,
                               const std::shared_ptr<OverlayManager>&             overlay_manager,
                               const std::shared_ptr<SystemInfoCache>&            system_info_cache,
                               const std::shared_ptr<AdditionalChunkWriter>&      additional_chunk_writer,
                               DDUberTraceApi*                                    uber_trace_api,
                               const std::shared_ptr<class UbertraceUserFactory>& ubertrace_factory,
                               const std::shared_ptr<Logger>&                     logger));

        /// @brief Destructor.
        ~RraTraceSourceImpl() override = default;

        void RouterConnectionStatusChanged(bool is_connected) override;

        void OnDriverConnected(const DDConnectionInfo& connection_info) override;

        void OnDriverDisconnected(DDConnectionId umd_connection_id) override;

        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) override;

        Result RequestAbortTrace(DDConnectionId umd_connection_id) override;

        Result RequestAbortProcessing() override;

        void PostSupportEvent(const RraTraceSourceSupportEventArgs& args) const;

        void RegisterSupportEvent(const RraTraceSourceSupportEvent& event) override;

        void RegisterStatusEvent(const TraceSourceStatusEvent& event) override;

        void RegisterTraceCompletionEvent(const TraceCompletionEvent& event) override;

        void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event) override;

        void QueryStatus() override;

        Result PrepareForDelayedCapture(DDConnectionId connection_id) override;

        Result RequestBeginTrace(DDConnectionId connection_id, uint32_t capture_mode) override;

        void GetSupportedCaptureModes(DDConnectionId connection_id, std::vector<uint32_t>& out_modes) override;

        RraTraceSourceConfig& GetConfig() override;

    private:
        const std::shared_ptr<SystemInfoCache> system_info_cache_;    ///< Object used to query the system info.
        std::shared_ptr<Logger>                logger_;               ///< Object used for debug logging.
        std::shared_ptr<RraClientFactory>      client_factory_;       ///< Factory used to create clients.
        BaseTriggerableTraceSource<RraClient>  base_trace_source_;    ///< Base trace source.
        std::mutex                             support_event_mutex_;  ///< Mutex guards support event.
        RraTraceSourceSupportEvent             support_event_{};      ///< Event for notifying support status.
    };
}  // namespace devtrace

#endif
