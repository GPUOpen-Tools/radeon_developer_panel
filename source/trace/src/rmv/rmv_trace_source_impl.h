// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for concrete implementation of the RMV trace source.

#ifndef RDP_SOURCE_TRACE_SRC_RMV_TRACE_SOURCE_IMPL_H_
#define RDP_SOURCE_TRACE_SRC_RMV_TRACE_SOURCE_IMPL_H_

#include <memory>

#include <ddApi.h>

#include "../base_trace_source/base_continuous_trace_source.h"
#include "chunk_writing.h"
#include "configurable.h"
#include "dipper.h"
#include "logging.h"
#include "rmv_trace_source.h"
#include "trace_io.h"

struct DDMemoryTraceApi;
struct DDApiRegistry;

namespace devtrace
{
    class SystemInfoCache;

    /// @brief RMV trace source client.
    class RmvClient final : public ContinuousClient<RmvTraceSourceConfig>
    {
    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @param [in] memory_trace_api The memory trace API.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        /// @param [in] enable_etw Whether ETW should be enabled for this client.
        RmvClient(ClientConnection                              conn_info,
                  ClientUtils<RmvTraceSourceConfig>&            client_utils,
                  DDMemoryTraceApi*                             memory_trace_api,
                  const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer,
                  bool                                          enable_etw);

        /// @brief Destructor.
        ~RmvClient() override;

        /// @brief Callback for writing status events.
        /// @param [in] object The object the event is for.
        /// @param [in] args The event arguments.
        static void OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args);

        /// @brief Gets the driver state that the client should initialize at.
        /// @return The driver state that the client should initialize at.
        [[nodiscard]] DD_DRIVER_STATE GetInitDriverState() override;

        /// @brief Initializes the client.
        /// @return The result of the initialization.
        [[nodiscard]] Result Initialize() override;

        /// @brief Disconnects the client.
        void Disconnect() override;

        /// @brief Handles a driver state change.
        /// @param [in] state The new driver state.
        [[nodiscard]] Result HandleDriverState(DD_DRIVER_STATE state) override;

        /// @brief Requests to abort the current trace.
        /// @return The result of the abort request.
        /// @note No-op for memory tracing; always returns kSuccess.
        [[nodiscard]] Result RequestAbortTrace() override;

        /// @brief Returns whether aborting traces is supported.
        /// @return true if aborting traces is supported, false otherwise.
        /// @note Always returns false for memory tracing.
        [[nodiscard]] bool IsAbortTraceSupported() override;

        /// @brief Requests that the trace be dumped to disk.
        /// @return The result of the dump request.
        [[nodiscard]] Result RequestDump() override;

        /// @brief Adds a marker with the given name to the trace in progress.
        /// @param [in] marker The name of the marker to add to the trace.
        /// @return The result of the add marker request
        [[nodiscard]] Result AddMarker(const std::string& marker) override;

    private:
        /// @brief Ends tracing.
        /// @return The result of ending tracing.
        [[nodiscard]] Result EndTracing();

        /// @brief Dumps the trace to disk.
        /// @return The result of the dump.
        [[nodiscard]] Result Dump();

        /// @brief Gets the current state of the memory tracing client.
        /// @return The current state of the memory tracing client.
        [[nodiscard]] ClientState GetMemoryTracingClientState() const;

        ClientConnection                       conn_info_{};                           ///< The connection info for the client.
        DDMemoryTraceApi*                      memory_trace_api_ = nullptr;            ///< The memory trace API.
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer_;               ///< The object that does additional chunk writing.
        ClientUtils<RmvTraceSourceConfig>&     client_utils_;                          ///< The utils that the client can use.
        bool                                   enable_etw_                   = false;  ///< Whether ETW should be enabled.
        bool                                   has_reached_post_device_init_ = false;  ///< true if the client reached post device init, false otherwise.
        ChunkWriterReservationId               chunk_reservation_            = kInvalidChunkWriterReservationId;  ///< Reservation for chunk writer.
        ProcessInfoChunk                       process_info_chunk_;                                               ///< The process info chunk.
    };

    /// @brief Factory for RMV clients.
    class RmvClientFactory final : public ClientFactory<RmvClient>
    {
    public:
        /// @brief Constructor.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] memory_trace_api The RMV API.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        RmvClientFactory(const std::shared_ptr<SystemInfoCache>&       system_info_cache,
                         DDMemoryTraceApi*                             memory_trace_api,
                         const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer);

        /// @brief Destructor.
        ~RmvClientFactory() override;

        /// @brief Creates a new RMV client.
        /// @param [in] info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @return The newly created RMV client.
        std::unique_ptr<RmvClient> CreateClient(const ClientConnection& info, ClientUtils<RmvTraceSourceConfig>& client_utils) override;

    private:
        std::shared_ptr<SystemInfoCache>       system_info_cache_;           ///< The system info cache.
        DDMemoryTraceApi*                      memory_trace_api_ = nullptr;  ///< The RMV API.
        std::shared_ptr<AdditionalChunkWriter> addl_chunk_writer_;           ///< The object that does additional chunk writing.
    };

    /// @brief Concrete implementation of the RMV trace source.
    class RmvTraceSourceImpl final : public RmvTraceSource
    {
    public:
        /// @brief Constructor.
        /// @param [in] stream_provider An object used to acquire writable streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        /// @param [in] memory_trace_api The memory trace API.
        /// @param [in] logger Object to use for any logging.
        DIP(RmvTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                               const std::shared_ptr<OverlayManager>&          overlay_manager,
                               const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                               const std::shared_ptr<AdditionalChunkWriter>&   additional_chunk_writer,
                               DDMemoryTraceApi*                               memory_trace_api,
                               const std::shared_ptr<Logger>&                  logger));

        /// @brief Destructor.
        ~RmvTraceSourceImpl() override = default;

        /// @brief Handles a new driver connection.
        /// @param [in] connection_info The new connection's information.
        void OnDriverConnected(const DDConnectionInfo& connection_info) override;

        /// @brief Handles a driver disconnection.
        /// @param [in] umd_connection_id The UMD connection ID that disconnected.
        void OnDriverDisconnected(DDConnectionId umd_connection_id) override;

        /// @brief Handles a driver state change.
        /// @param [in] umd_connection_id The UMD connection ID that changed state.
        /// @param [in] state The new driver state.
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) override;

        /// @brief Requests to abort the current trace.
        /// @return The result of the abort request.
        /// @note No-op for memory tracing; always returns kSuccess.
        Result RequestAbortTrace(DDConnectionId umd_connection_id) override;

        /// @brief Requests to abort the current processing.
        /// @return The result of the abort request.
        /// @note No-op for memory tracing; always returns kSuccess.
        Result RequestAbortProcessing() override;

        /// @brief Registers a status event listener.
        /// @param [in] event The event to register.
        void RegisterStatusEvent(const TraceSourceStatusEvent& event) override;

        /// @brief Registers a trace completion event listener.
        /// @param [in] event The event to register.
        void RegisterTraceCompletionEvent(const TraceCompletionEvent& event) override;

        /// @brief Registers a trace capture progress event listener.
        /// @param [in] event The event to register.
        void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event) override;

        /// @brief Force a broadcast of current trace source status to all listeners.
        void QueryStatus() override;

        /// @brief Requests that the trace be dumped to disk.
        /// @param [in] connection_id The identifier of the connection to dump.
        /// @return kSuccess if the dump was successful.
        Result RequestDump(DDConnectionId connection_id) override;

        /// @brief Adds a marker with the given name to the trace in progress.
        /// @param [in] connection_id The identifier of the connection to add a marker for.
        /// @param [in] marker The name of the marker to add to the trace.
        /// @return kSuccess if the marker was successfully added to the trace.
        Result AddMarker(DDConnectionId connection_id, const std::string& marker) override;

        /// @brief Gets the trace source configuration.
        /// @return The trace source configuration.
        RmvTraceSourceConfig& GetConfig() override;

    private:
        std::shared_ptr<Logger>              logger_;             ///< Object used for debug logging.
        BaseContinuousTraceSource<RmvClient> base_trace_source_;  ///< Base trace source.
    };

}  // namespace devtrace

#endif
