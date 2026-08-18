// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for trace source adapter.

#ifndef RDP_SOURCE_API_CAPTURE_TRACE_SOURCE_ADAPTER_H_
#define RDP_SOURCE_API_CAPTURE_TRACE_SOURCE_ADAPTER_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include <client_connection_manager.h>

#include "RdpCaptureApi.h"
#include "capture_params.h"
#include "trace_source_provider.h"

namespace devtrace
{
    class TraceSource;
    class ClientConnectionManagerV2;
}  // namespace devtrace

/// @brief Adapts a trace source to the RDP capture API.
class TraceSourceAdapter : public AllTraceSourceProvider
{
public:
    /// @brief Constructor.
    /// @param [in] feature The feature that the trace source is for.
    /// @param [in] trace_source The trace source to adapt.
    /// @param [in] stream_provider Provides streams
    /// @param [in] conn_manager The connection manager.
    /// @param [in] callback The trace finished callback.
    template <typename T>
    TraceSourceAdapter(RdpCaptureFeature                                           feature,
                       const std::shared_ptr<T>&                                   trace_source,
                       const std::shared_ptr<class MemoryReadWriteStreamProvider>& stream_provider,
                       std::unique_ptr<devtrace::ClientConnectionManagerV2>        conn_manager);

    static void OnTraceCompletionEvent(void* object, const devtrace::TraceCompletionEventArgs& args);
    static void OnTraceStatusEvent(void* object, const devtrace::TraceSourceStatusEventArgs& args);
    static void OnTraceCaptureProgressEvent(void* object, const devtrace::TraceCaptureProgressEventArgs& args);

private:
    /// @brief Performs any trace source configuration that's needed.
    /// @tparam T The trace source type.
    /// @param [in] trace_source The trace source to configure.
    template <typename T>
    void ConfigureTraceSource(const std::shared_ptr<T>& trace_source);

public:
    /// @brief Initializes the adapter.
    /// @return true if initialization was successful, false otherwise.
    bool Initialize();

    /// @brief Binds to the trace source.
    void BindToTraceSource();

    /// @brief Configures the trace source if it is RGP.
    /// @param [in] params The params to use to configure the trace source with.
    /// @return The result of configuring the trace source.
    RdpCaptureResult ConfigureRgp(const RdpCaptureProfilingEnableParams& params);

    /// @brief Configures the trace source if it is RGD.
    /// @param [in] params The params to use to configure the trace source with.
    /// @return The result of configuring the trace source.
    RdpCaptureResult ConfigureRgd(const RdpCaptureCrashAnalysisEnableParams& params);

    /// @brief Configures the trace source if it is RRA.
    /// @param [in] params The params to use to configure the trace source with.
    /// @return kRdpCaptureResultUnsupported if marker capture is requested but not supported by the driver.
    RdpCaptureResult ConfigureRra(const RdpCaptureRaytracingEnableParams& params);

    /// @brief Sets whether the adapter is enabled.
    /// @param [] enabled
    void SetEnabled(bool enabled);

    /// @brief Sets the callback to call when a trace is finished.
    /// @param [in] callback The callback to call.
    void SetTraceFinishedCallback(const RdpFeatureTraceFinishedCallback& callback);

    /// @brief Sets the callback to call when the feature status changes.
    /// @param [in] callback The callback to call.
    void SetStatusCallback(const RdpCaptureFeatureStatusCallback& callback);

    /// @brief Sets the callback to call when capture progress updates.
    /// @param [in] callback The callback to call.
    void SetProgressCallback(const RdpCaptureFeatureProgressCallback& callback);

    /// @brief Gets the feature stage.
    /// @param [in] timeout_ms The timeout to wait for the trace source to transition away from capturing.
    /// @return The feature stage.
    RdpCaptureFeatureStage GetFeatureStage(uint64_t timeout_ms);

    /// @brief Gets the API connections for this feature.
    /// @param [out] connections The connections.
    /// @param [out] num_connections The number of connections.
    void GetFeatureApiConnections(RdpCaptureApiConnection** connections, uint64_t* num_connections);

private:
    /// @brief Returns the trace source connections (but sorted by ID).
    /// @return The trace source connections.
    [[nodiscard]] std::unordered_map<uint16_t, devtrace::Api> GetTraceSourceConnections() const;

    /// @brief Converts an RDP connection id to a devtrace connection id.
    /// @param [in] connection The id of the connection to convert.
    /// @return The devtrace id of the connection.
    uint16_t GetConnectionId(RdpCaptureApiConnectionId connection) const;

public:
    /// @brief Requests beginning a trace.
    /// @param [in] connection The connection to begin a trace on.
    /// @return The result of beginning a trace.
    RdpCaptureResult BeginTrace(RdpCaptureApiConnectionId connection);

    /// @brief Aborts a trace.
    /// @return The result of aborting the trace.
    RdpCaptureResult AbortTrace();

    /// @brief Inserts a memory tracing snapshot.
    /// @param [in] connection The connection to insert a snapshot for.
    /// @param [in] snapshot_name The name of the snapshot to add (null-terminated).
    /// @return The result of inserting the snapshot.
    RdpCaptureResult InsertSnapshot(RdpCaptureApiConnectionId connection, const char* snapshot_name);

    /// @brief Dumps a trace.
    /// @param [in] connection The connection to dump a trace for.
    /// @return The result of dumping the trace.
    RdpCaptureResult DumpTrace(RdpCaptureApiConnectionId connection);

    /// @brief Sets the parameters for the Raytracing feature.
    /// @param [in] params The parameters to set.
    void SetProfilingParams(const RdpCaptureProfilingParams* params);

    /// @brief Sets the parameters for the Raytracing feature.
    /// @param [in] params The parameters to set.
    void SetRaytracingParams(const RdpCaptureRaytracingParams* params);

private:
    RdpCaptureFeature                                    feature_;          ///< The feature that the trace source is for.
    std::shared_ptr<devtrace::TraceSource>               trace_source_;     ///< The trace source to adapt.
    std::shared_ptr<class MemoryReadWriteStreamProvider> stream_provider_;  ///< Provides streams.
    std::unique_ptr<devtrace::ClientConnectionManagerV2> conn_manager_;     ///< The connection manager.
    std::unordered_map<DDConnectionId, devtrace::Api>    connection_map_;   ///< Map of RDP connection IDs to devtrace connection.
    devtrace::TraceSourceStatus                          current_status_;   ///< The current status of the trace source.

    std::mutex                        callback_mutex_;       ///< Mutex that guards the callbacks.
    RdpFeatureTraceFinishedCallback   callback_{};           ///< The trace finished callback to call.
    RdpCaptureFeatureStatusCallback   status_callback_{};    ///< The status change callback.
    RdpCaptureFeatureProgressCallback progress_callback_{};  ///< The progress update callback.
    RdpCaptureFeatureStage            current_stage_          = RdpCaptureFeatureStage::kRdpCaptureFeatureStageUnknown;  ///< The current feature stage.
    RdpCaptureDetailedStage           current_detailed_stage_ = kRdpCaptureDetailedStageUnknown;                         ///< The current detailed stage.
    std::atomic<uint32_t>             begin_trace_capture_mode_{0};  ///< Capture mode to pass to RequestBeginTrace (set via SetProfilingParams).
};

template <typename T>
TraceSourceAdapter::TraceSourceAdapter(RdpCaptureFeature                                           feature,
                                       const std::shared_ptr<T>&                                   trace_source,
                                       const std::shared_ptr<class MemoryReadWriteStreamProvider>& stream_provider,
                                       std::unique_ptr<devtrace::ClientConnectionManagerV2>        conn_manager)
    : AllTraceSourceProvider(trace_source)
    , feature_(feature)
    , trace_source_(trace_source)
    , stream_provider_(stream_provider)
    , conn_manager_(std::move(conn_manager))
{
    ConfigureTraceSource<T>(trace_source);
}

template <typename T>
void TraceSourceAdapter::ConfigureTraceSource([[maybe_unused]] const std::shared_ptr<T>& trace_source)
{
}

template <>
inline void TraceSourceAdapter::ConfigureTraceSource(const std::shared_ptr<devtrace::RgpTraceSource>& trace_source)
{
    RdpCaptureProfilingParams params;
    GetDefaultProfilingParams(&params);
    ApplyProfilingParams(&params, trace_source);
}

template <>
inline void TraceSourceAdapter::ConfigureTraceSource(const std::shared_ptr<devtrace::RraTraceSource>& trace_source)
{
    RdpCaptureRaytracingParams params;
    GetDefaultRaytracingParams(&params);
    ApplyRaytracingParams(&params, trace_source);
}

template <>
inline void TraceSourceAdapter::ConfigureTraceSource(const std::shared_ptr<devtrace::RgdTraceSource>& trace_source)
{
    trace_source->GetConfig().generate_text_summary = false;
    trace_source->GetConfig().generate_json_summary = false;
}

#endif
