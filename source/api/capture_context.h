// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for capture context.

#ifndef RDP_SOURCE_API_CAPTURE_CAPTURE_CONTEXT_H_
#define RDP_SOURCE_API_CAPTURE_CAPTURE_CONTEXT_H_

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <client_connection_manager.h>
#include <dd_driver_utils_api.h>
#include <dipper.h>

#include "RdpCaptureApi.h"
#include "api_blocklist.h"
#include "api_trace_io.h"

namespace devtrace
{
    class OverlayManager;
    class TraceSource;
    class SystemInfoCache;
    class Logger;
}  // namespace devtrace

/// @brief RDP capture context.
class RdpCaptureContextImpl
{
public:
    explicit RdpCaptureContextImpl(const RdpCaptureContextInitParams& init_params);

    /// @brief Destructor.
    ~RdpCaptureContextImpl();

    /// @brief Attempts to initialize the capture context.
    /// @return The result of initialization.
    RdpCaptureResult Initialize();

private:
    /// @brief Resolves dependencies.
    /// @param [in] container The container to register / resolve dependencies from.
    void ResolveDependencies(dipper::Container& container);

    template <typename TraceSource>
    void CreateAdapter(RdpCaptureFeature feature);

    /// @brief Initializes any components that need it.
    /// @return true if initialization is successful, false otherwise.
    bool InitializeComponents();

    /// @brief Queries the system info.
    /// @return true if the query was successful, false otherwise.
    bool QuerySystemInfo();

public:
    /// @brief Enables a feature.
    /// @param [in] feature_params The parameters to enable the feature with.
    /// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not.
    RdpCaptureResult EnableFeature(const RdpCaptureFeatureEnableParams* feature_params);

private:
    /// @brief Configures the adapter.
    /// @param [in] params The params to configure the feature with.
    /// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not.
    RdpCaptureResult EnableAdapter(const RdpCaptureFeatureEnableParams& params);

    /// @brief Enables the driver feature flag corresponding to the given capture feature.
    /// @param [in] feature The capture feature to enable.
    /// @return true if the driver feature was set successfully or no driver feature is needed.
    bool EnableDriverFeature(RdpCaptureFeature feature);

    /// @brief Disables the driver feature flag corresponding to the given capture feature.
    /// @param [in] feature The capture feature to disable.
    /// @return true if the driver feature was reset successfully or no driver feature is needed.
    bool DisableDriverFeature(RdpCaptureFeature feature);

public:
    /// @brief Enables a feature.
    /// @param [in] feature The feature to disable.
    /// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not.
    RdpCaptureResult DisableFeature(RdpCaptureFeature feature);

public:
    /// @brief Gets information about the connected process.
    ///
    /// A context can only connect to a single process at a time.

    /// @param [out] out_process_info The information about the connected process.
    void GetProcessInfo(RdpCaptureProcessInfo* out_process_info);

    /// @brief Gets the current stage of the feature.
    /// @param [in] feature The feature to get the stage for.
    /// @param [in] timeout_ms The time to wait for the feature to transition away from kRdpCaptureFeatureStageCapturing.
    /// @return The current stage of the feature after waiting the timeout.
    RdpCaptureFeatureStage GetFeatureStage(RdpCaptureFeature feature, uint64_t timeout_ms);

    /// @brief Gets the current API connections for the given feature.
    ///
    /// A RdpCaptureApiConnection is created once a feature successfully performs initialization for a connection accepted through the app filter.
    /// If initialization fails or the feature does not support the API of the connection, a RdpCaptureApiConnection will not be created.
    /// @param [in] feature The feature to get the API connections for.
    /// @param [out] connections The current connections for the feature (should be freed with RdpCaptureFnFree()).
    /// @param [out] num_connections The number of current connections in the @param connections array.
    void GetApiConnections(RdpCaptureFeature feature, RdpCaptureApiConnection** connections, uint64_t* num_connections);

    /// @brief Sets the description that will be saved in traces captured by the feature.
    ///
    /// This should to be called before a feature enters the kRdpCaptureFeatureStageBusy stage.
    /// @param [in] feature The feature to set the trace description for.
    /// @param [in] desc A null-terminated, UTF-8 encoded string (newlines supported) that will be saved with traces taken by the feature.
    void SetTraceDesc(RdpCaptureFeature feature, const char* desc);

    /// @brief Begins a trace.
    /// @param [in] connection The connection to begin a trace for.
    /// @param [in] feature The feature to begin a trace for.
    /// @return The result of beginning the trace.
    RdpCaptureResult BeginTrace(RdpCaptureApiConnectionId connection, RdpCaptureFeature feature);

    /// @brief Aborts a trace.
    /// @param [in] feature The feature to abort a trace for.
    /// @return The result of aborting a trace.
    RdpCaptureResult AbortTrace(RdpCaptureFeature feature);

    /// @brief Inserts a memory tracing snapshot.
    /// @param [in] connection The connection to insert a snapshot for.
    /// @param [in] snapshot_name The name of the snapshot to add (null-terminated).
    /// @return The result of inserting the snapshot.
    RdpCaptureResult InsertSnapshot(RdpCaptureApiConnectionId connection, const char* snapshot_name);

    /// @brief Dumps a memory trace.
    /// @param [in] connection The connection to dump a trace for.
    /// @return The result of dumping a trace.
    RdpCaptureResult DumpTrace(RdpCaptureApiConnectionId connection);

    /// @brief Sets the parameters for the Raytracing feature.
    /// @param [in] params The parameters to set.
    void SetProfilingParams(const RdpCaptureProfilingParams* params);

    /// @brief Sets the parameters for the Raytracing feature.
    /// @param [in] params The parameters to set.
    void SetRaytracingParams(const RdpCaptureRaytracingParams* params);

    /// @brief Gets aggregated system info (OS, driver, CPUs, GPUs) matching the RDP UI System Info panel.
    /// @param [out] out_info The system info struct to populate.
    /// @return kRdpCaptureResultSuccess on success, error code otherwise.
    RdpCaptureResult GetSystemInfo(RdpCaptureSystemInfo* out_info);

    /// @brief Frees the nested allocations populated by GetSystemInfo.
    /// @param [in,out] info The system info struct to release. May be nullptr.
    static void FreeSystemInfo(RdpCaptureSystemInfo* info);

    /// @brief Gets the details of the supported clock modes for the given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to get the supported clock modes for.
    /// @param [out] modes The supported clock modes for the GPU (should be freed with RdpCaptureFnFree()).
    /// @param [out] num_modes The number of modes in the @param modes array.
    void GetGpuClockModes(uint64_t gpu_index, RdpCaptureGpuClockModeDetails** modes, uint64_t* num_modes);

    /// @brief Queries the current clock mode of a given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to query the clock mode for.
    /// @param [out] mode The current clock mode for the GPU.
    /// @return The result of querying the current clock mode for the GPU.
    RdpCaptureResult QueryGpuCurrentClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode* mode);

    /// @brief Sets the current clock mode of a given GPU.
    /// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to set the clock mode for.
    /// @param [in] mode The desired clock mode for the GPU.
    RdpCaptureResult SetCurrentGpuClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode mode);

    /// @brief Adds a process name pattern to the blocklist.
    /// @param [in] pattern The wildcard pattern to block (e.g. "svchost.exe", "MyApp*").
    /// @return kRdpCaptureResultSuccess, or kRdpCaptureResultInvalidParams if pattern is null/empty.
    RdpCaptureResult AddBlocklistEntry(const char* pattern);

    /// @brief Removes a process name pattern from the blocklist.
    /// @param [in] pattern The pattern to remove.
    /// @return kRdpCaptureResultSuccess, kRdpCaptureResultNotFound, or kRdpCaptureResultInvalidParams.
    RdpCaptureResult RemoveBlocklistEntry(const char* pattern);

    /// @brief Clears all blocklist entries, including built-in defaults.
    void ClearBlocklist();

    /// @brief Returns all current blocklist patterns.
    /// @param [out] entries     Array of null-terminated pattern strings (free with FreeBlocklistEntries).
    /// @param [out] num_entries Number of entries written.
    void GetBlocklistEntries(char*** entries, uint64_t* num_entries) const;

    /// @brief Loads additional blocklist entries from a text file.
    /// @param [in] file_path Path to the blocklist file.
    /// @return kRdpCaptureResultSuccess, kRdpCaptureResultNotFound, or kRdpCaptureResultInvalidParams.
    RdpCaptureResult LoadBlocklistFile(const char* file_path);

    /// @brief Registers a callback invoked whenever a process is blocked by the blocklist.
    /// @param [in] callback  Function called with (user_data, full_process_path, process_id). Pass nullptr to unregister.
    /// @param [in] user_data Opaque pointer forwarded to the callback.
    void SetBlocklistBlockedCallback(void (*callback)(void* user_data, const char* process_path, uint32_t process_id), void* user_data);

private:
    RdpCaptureContextInitParams       init_params_;  ///< The initialization parameters.
    std::shared_ptr<dipper::Resolver> resolver_;     ///< The resolver to get dependencies from.

    std::unique_ptr<devtrace::Logger> logger_;  ///< Logger to use.

    std::unique_ptr<class ApiTool>       tool_;                        ///< The tool to use to connect to a system.
    std::unique_ptr<class ClientManager> client_manager_;              ///< Manages filtering clients.
    struct DDDriverUtilsApi*             driver_utils_api_ = nullptr;  ///< The driver utils API for feature enable/disable.

    std::shared_ptr<devtrace::SystemInfoCache> sys_info_cache_;  ///< System info cache.
    system_info_utils::SystemInfo              sys_info_;        ///< The queried system info.
    std::unique_ptr<class ApiClocks>           clocks_;          ///< The object used to manage device clocks.

    std::mutex                                                                       state_mutex;        ///< Mutex that guards internal state that needs it.
    std::unordered_map<RdpCaptureFeature, std::shared_ptr<class TraceSourceAdapter>> adapters_;          ///< The trace source adapters.
    std::unordered_set<RdpCaptureFeature>                                            enabled_features_;  ///< The currently enabled features.

    std::unique_ptr<ApiBlocklist> blocklist_;  ///< The application blocklist.
};

template <typename TraceSource>
void RdpCaptureContextImpl::CreateAdapter(RdpCaptureFeature feature)
{
    adapters_.insert({feature,
                      std::make_shared<TraceSourceAdapter>(feature,
                                                           resolver_->Resolve<std::shared_ptr<TraceSource>>(),
                                                           resolver_->Resolve<std::shared_ptr<MemoryReadWriteStreamProvider>>(),
                                                           resolver_->Resolve<std::unique_ptr<devtrace::ClientConnectionManagerV2>>())});
}

#endif
