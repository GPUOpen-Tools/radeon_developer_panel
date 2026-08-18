// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  An API for the developer mode driver to initialize driver protocols.
///
/// Can be used by applications to write RGP profiles/RMV traces of themselves.

#ifndef RDP_SOURCE_API_CAPTURE_RDP_CAPTURE_API_H_
#define RDP_SOURCE_API_CAPTURE_RDP_CAPTURE_API_H_

#include <stdbool.h>
#include <stdint.h>

#include "rdp_capture_api_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Handle to context that controls Developer Mode capture.
typedef void* RdpCaptureContext;

/// @brief Result codes.
typedef enum
{
    kRdpCaptureResultSuccess  = 0,  ///< The operation was successful.
    kRdpCaptureResultNotReady = 1,  ///< The operation was unavailable due to ready state.

    kRdpCaptureResultUnknown         = -1,  ///< The operation's result was unknown.
    kRdpCaptureResultFailure         = -2,  ///< The operation was not successful.
    kRdpCaptureResultUnsupported     = -3,  ///< The operation was unsupported.
    kRdpCaptureResultNotFound        = -4,  ///< Something required by the operation could not be found.
    kRdpCaptureResultAborted         = -5,  ///< The operation was aborted.
    kRdpCaptureResultInvalidParams   = -6,  ///< The parameters were invalid.
    kRdpCaptureResultVersionMismatch = -7   ///< There was a version mismatch between the loaded library.
} RdpCaptureResult;

/// @brief Various stages of the capture progress that each feature will step through as it captures data.
typedef enum
{
    kRdpCaptureFeatureStageUnknown               = 0,  ///< The status of the feature is unknown.
    kRdpCaptureFeatureStageDisconnected          = 1,  ///< Not currently connected to an application.
    kRdpCaptureFeatureStageReadyForCapture       = 2,  ///< Tracing is ready, but is not in progress.
    kRdpCaptureFeatureStageWaitingToBeginCapture = 3,  ///< A capture is pending, but is waiting  to proceed.
    kRdpCaptureFeatureStageCapturing             = 4,  ///< The request to trace has been sent and the trace is being taken.
    kRdpCaptureFeatureStageDone                  = 5,  ///< A trace completed successfully, but no more traces can be taken.
    kRdpCaptureFeatureStageEncounteredError      = 6,  ///< An error was encountered and no further traces can be.
    kRdpCaptureFeatureStageBusy                  = 7,  ///< The driver is currently busy with an operation that must complete before a trace can be taken.

    kRdpCaptureFeatureStageDisabled            = -1,  ///< The feature is disabled, but the reason is unknown.
    kRdpCaptureFeatureStageDisabledFromError   = -2,  ///< An unrecoverable error was encountered, so the feature was disabled.
    kRdpCaptureFeatureStageHardwareUnsupported = -3,  ///< The feature required hardware support that was not available.
    kRdpCaptureFeatureStageOsUnsupported       = -4,  ///< The feature required operating system support that was not available.
    kRdpCaptureFeatureStageDriverUnsupported   = -5,  ///< The feature required driver support that was not available.
    kRdpCaptureFeatureStageApiUnsupported      = -6   ///< The feature required API support that was not available.
} RdpCaptureFeatureStage;

/// @brief APIs that are used by application connections.
typedef enum
{
    kRdpCaptureGpuApiUnknown = 0,  ///< Unknown API. If the client has connected, something has likely gone wrong.

    kRdpCaptureGpuApiDirectX12 = 1,  ///< DirectX12.
    kRdpCaptureGpuApiDirectX11 = 2,  ///< DirectX11.
    // DirectX 10 and 11 will both be reported as DirectX 11
    kRdpCaptureGpuApiDirectX9 = 3,  ///< DirectX9.

    kRdpCaptureGpuApiVulkan = 4,  ///< Any version of Vulkan.
    kRdpCaptureGpuApiOpenCl = 5,  ///< Any version of OpenCL.
    kRdpCaptureGpuApiHip    = 6,  ///< Any version of HIP.
    kRdpCaptureGpuApiOpenGl = 7   ///< Any version of OpenGL.
} RdpCaptureGpuApi;

/// @brief The different features that can be enabled.
typedef enum
{
    kRdpCaptureFeatureUnknown       = 0,
    kRdpCaptureFeatureProfiling     = 1,
    kRdpCaptureFeatureMemoryTrace   = 2,
    kRdpCaptureFeatureRaytracing    = 3,
    kRdpCaptureFeatureCrashAnalysis = 4
} RdpCaptureFeature;

/// @brief Bitmask for RdpCaptureProfilingEnableParams flags.
typedef enum
{
    kRdpCaptureProfilingEnableParamFlagNone                        = 0,  ///< No flags.
    kRdpCaptureProfilingEnableParamFlagUseFrameIndexCapture        = 1,  ///< Enables automatic frame index capture.
    kRdpCaptureProfilingEnableParamFlagUseDispatchIndexCapture     = 2,  ///< Enables automatic dispatch capture.
    kRdpCaptureProfilingEnableParamFlagEnableShaderInstrumentation = 4,  ///< Enables collecting detailed data about shaders.

} RdpCaptureProfilingEnableParamFlags;

/// @brief Enable parameters for profiling.
typedef struct RdpCaptureProfilingEnableParams
{
    uint32_t flags;  ///< RdpCaptureProfilingEnableParamFlags.

    uint32_t frame_capture_index;        ///< The frame index to capture.
    uint32_t dispatch_start_index;       ///< The start index for dispatch captures.
    uint32_t dispatch_count;             ///< The number of dispatches to capture,
    uint32_t dispatch_capture_delay_ms;  ///< Delay in milliseconds before dispatch auto-capture starts.
} RdpCaptureProfilingEnableParams;

/// @brief Enable parameters for memory trace.
typedef struct RdpCaptureMemoryTraceEnableParams
{
    uint32_t reserved;  ///< Reserved for future use.
} RdpCaptureMemoryTraceEnableParams;

/// @brief Bitmask for RdpCaptureRaytracingEnableParams flags.
typedef enum
{
    kRdpCaptureRaytracingEnableParamFlagNone                = 0,  ///< No flags.
    kRdpCaptureRaytracingEnableParamFlagEnableMarkerCapture = 1,  ///< Enables marker-based capture instead of frame-based.

} RdpCaptureRaytracingEnableParamFlags;

/// @brief Enable parameters for raytracing.
typedef struct RdpCaptureRaytracingEnableParams
{
    uint32_t flags;  ///< RdpCaptureRaytracingEnableParamFlags.

    const char* marker_begin_string;  ///< The marker string that starts the capture (only used if marker capture is enabled).
    const char* marker_end_string;    ///< The marker string that ends the capture (only used if marker capture is enabled).
} RdpCaptureRaytracingEnableParams;

/// @brief Bitmask for RdpCaptureCrashAnalysisEnableParams flags.
typedef enum
{
    kRdpCaptureCrashAnalysisEnableParamFlagNone                        = 0,       ///< No flags.
    kRdpCaptureCrashAnalysisEnableParamFlagEnableEnhancedCrashAnalysis = 1 << 0,  ///< Enables enhanced crash analysis (may affect connected app performance).
    kRdpCaptureCrashAnalysisEnableParamFlagGenerateTextSummary = 1 << 1,  ///< Generate a text summary alongside the .rgd file (consumed by downstream tooling).
    kRdpCaptureCrashAnalysisEnableParamFlagGenerateJsonSummary = 1 << 2,  ///< Generate a JSON summary alongside the .rgd file (consumed by downstream tooling).
    kRdpCaptureCrashAnalysisEnableParamFlagShowMarkerSource    = 1 << 3,  ///< Display execution-marker source information in the summary.
    kRdpCaptureCrashAnalysisEnableParamFlagExpandMarkers       = 1 << 4,  ///< Expand all execution-marker nodes in the summary.
    kRdpCaptureCrashAnalysisEnableParamFlagPdbIncludeSubfolders = 1 << 5,  ///< Recurse into subfolders when resolving DXC shader PDBs.
    kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveSgprs     = 1 << 6,  ///< Collect wave SGPR data during enhanced crash analysis.
    kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveVgprs     = 1 << 7,  ///< Collect wave VGPR data during enhanced crash analysis.

} RdpCaptureCrashAnalysisParamFlags;

/// @brief Enable parameters for crash analysis.
typedef struct RdpCaptureCrashAnalysisEnableParams
{
    uint32_t           flags;                 ///< RdpCaptureCrashAnalysisParamFlags.
    const char* const* pdb_search_paths;      ///< Optional array of DXC shader PDB search paths. May be NULL when num_pdb_search_paths is 0.
    uint64_t           num_pdb_search_paths;  ///< Number of entries in pdb_search_paths.
} RdpCaptureCrashAnalysisEnableParams;

typedef void* RdpCaptureApiConnectionId;  ///< Identifiers for application API connections.

/// @brief A constant that can be used to choose the first connection automatically.
#define RDP_CAPTURE_API_FIRST_CONNECTION_ID ((RdpCaptureApiConnectionId)0)

/// @brief A connection to an application through a particular API that can have traces captured from it.
typedef struct RdpCaptureApiConnection
{
    RdpCaptureApiConnectionId id;   ///< The unique identifier of the connection.
    RdpCaptureGpuApi          api;  ///< The API being used by the connection.
} RdpCaptureApiConnection;

/// @brief Callback to call when a trace finishes.
typedef struct RdpFeatureTraceFinishedCallback
{
    /// @brief The callback to call when a trace is finished.
    /// @param [in] user_data The user data to call with the callback.
    /// @param [in] feature The feature that produced the trace.
    /// @param [in] connection The identifier of the connection that produced the trace.
    /// @param [in] result The result of the trace.
    /// @param [in] size The size in bytes of the buffer.
    /// @param [in] data The data of the completed trace. Will be freed after this callback is called.
    void (*trace_finished)(void*                     user_data,
                           RdpCaptureFeature         feature,
                           RdpCaptureApiConnectionId connection,
                           RdpCaptureResult          result,
                           uint64_t                  size,
                           const uint8_t*            data);

    void* user_data;  ///< The user data to call with the callback.
} RdpFeatureTraceFinishedCallback;

/// @brief Detailed capture stages reported through the status callback.
///
/// These provide finer granularity than RdpCaptureFeatureStage and are only reported via the
/// RdpCaptureFeatureStatusCallback. The polling API (RdpCaptureFnGetFeatureStage) continues to
/// report the coarser RdpCaptureFeatureStage values for backwards compatibility.
typedef enum
{
    kRdpCaptureDetailedStageUnknown               = 0,   ///< The status of the feature is unknown.
    kRdpCaptureDetailedStageDisconnected          = 1,   ///< Not currently connected to an application.
    kRdpCaptureDetailedStageReadyForCapture       = 2,   ///< Tracing is ready, but is not in progress.
    kRdpCaptureDetailedStageWaitingToBeginCapture = 3,   ///< A capture is pending, but is waiting to proceed.
    kRdpCaptureDetailedStageCapturing             = 4,   ///< The trace is actively being captured.
    kRdpCaptureDetailedStageDumping               = 5,   ///< The trace is streaming data from GPU to CPU.
    kRdpCaptureDetailedStageProcessing            = 6,   ///< Post-processing is being performed on the trace.
    kRdpCaptureDetailedStageDone                  = 7,   ///< A trace completed successfully.
    kRdpCaptureDetailedStageEncounteredError      = 8,   ///< An error was encountered.
    kRdpCaptureDetailedStageBusy                  = 9,   ///< The driver is currently busy.
    kRdpCaptureDetailedStageDisabled              = -1,  ///< The feature is disabled.
    kRdpCaptureDetailedStageDisabledFromError     = -2,  ///< An unrecoverable error disabled the feature.
    kRdpCaptureDetailedStageHardwareUnsupported   = -3,  ///< Hardware support was not available.
    kRdpCaptureDetailedStageOsUnsupported         = -4,  ///< OS support was not available.
    kRdpCaptureDetailedStageDriverUnsupported     = -5,  ///< Driver support was not available.
    kRdpCaptureDetailedStageApiUnsupported        = -6   ///< API support was not available.
} RdpCaptureDetailedStage;

/// @brief Callback that gets called when the feature stage changes.
///
/// This callback may be invoked from internal threads. Implementations must be thread-safe,
/// return quickly, and not call back into the RDP Capture API from within the callback.
/// The callback will not be called concurrently for the same feature, but may be called
/// concurrently for different features.
typedef struct RdpCaptureFeatureStatusCallback
{
    /// @brief Called when the feature transitions to a new stage.
    /// @param [in] user_data The user data passed to the callback.
    /// @param [in] feature The feature whose stage changed.
    /// @param [in] new_stage The new detailed stage of the feature.
    /// @param [in] old_stage The previous detailed stage of the feature.
    void (*status_changed)(void* user_data, RdpCaptureFeature feature, RdpCaptureDetailedStage new_stage, RdpCaptureDetailedStage old_stage);

    void* user_data;  ///< The user data to call with the callback.
} RdpCaptureFeatureStatusCallback;

/// @brief Progress information passed to the progress callback.
///
/// All string pointers are only valid for the duration of the callback invocation.
typedef struct RdpCaptureProgressInfo
{
    RdpCaptureFeature       feature;              ///< The feature reporting progress.
    RdpCaptureDetailedStage stage;                ///< The current detailed stage.
    float                   progress;             ///< A value in [0.0, 1.0] representing overall progress of the current stage.
    uint64_t                num_bytes_dumped;     ///< The number of bytes dumped so far (relevant during dumping stage).
    uint64_t                total_bytes_to_dump;  ///< The total number of bytes expected to be dumped (0 if unknown).
    const char*             progress_text;        ///< Human-readable progress description. Valid only during callback. May be NULL.
} RdpCaptureProgressInfo;

/// @brief Callback that gets called when capture progress is updated.
///
/// This callback may be invoked from internal threads. Implementations must be thread-safe,
/// return quickly, and not call back into the RDP Capture API from within the callback.
typedef struct RdpCaptureFeatureProgressCallback
{
    /// @brief Called when progress is updated for a feature.
    /// @param [in] user_data The user data passed to the callback.
    /// @param [in] progress_info The current progress information.
    void (*progress_updated)(void* user_data, const RdpCaptureProgressInfo* progress_info);

    void* user_data;  ///< The user data to call with the callback.
} RdpCaptureFeatureProgressCallback;

/// @brief Enable parameters for a feature.
typedef struct RdpCaptureFeatureEnableParams
{
    RdpCaptureFeature               feature;                  ///< The feature to enable.
    RdpFeatureTraceFinishedCallback trace_finished_callback;  ///< Callback to call when a trace is finished.

    RdpCaptureFeatureStatusCallback   status_callback;    ///< Callback to call when the feature stage changes. May be zeroed if not needed.
    RdpCaptureFeatureProgressCallback progress_callback;  ///< Callback to call when capture progress updates. May be zeroed if not needed.

    union
    {
        RdpCaptureProfilingEnableParams     profiling;       ///< Profiling enable params.
        RdpCaptureMemoryTraceEnableParams   memory_trace;    ///< Memory Trace enable params.
        RdpCaptureRaytracingEnableParams    raytracing;      ///< Raytracing enable params.
        RdpCaptureCrashAnalysisEnableParams crash_analysis;  ///< Crash analysis enable params.
    } body;

} RdpCaptureFeatureEnableParams;

/// @brief Size of the process path buffer in RdpCaptureProcessInfo.
#define RDP_CAPTURE_PROCESS_PATH_SIZE 1024

/// Information about the connected process.
typedef struct RdpCaptureProcessInfo
{
    char     process_path[RDP_CAPTURE_PROCESS_PATH_SIZE];  ///< The absolute path of the executable for the process.
    uint32_t process_id;                                   ///< The id of the connected process.
} RdpCaptureProcessInfo;

/// @brief Filter callback for applications.
typedef struct RdpCaptureAppFilter
{
    /// @brief This callback is called to determine which process / APIs a context should connect to.
    ///
    /// If this function pointer is NULL, the default behavior will be to connect to all APIs for the current process.
    ///
    /// If no process is connected, all eligible connections on the system will pass through this callback. Each connection will be on it's own thread.
    /// If there is not a connected process and 1 is returned from this callback, the process specified in process_info will become the connected process.
    /// If there is a connected process, only connections made by that process will be sent to this callback until that process disconnects.
    /// After a process disconnects, this callback will be called for all eligible connections on the system again to provide an opportunity to connect to new
    /// processes.
    ///
    /// @param [in] user_data The user data passed to the callback.
    /// @param [in] process_info Information about the process.
    /// @param [in] api The API being used by the connection.
    /// @return 1 if the process should connect, 0 otherwise.
    uint8_t (*filter)(void* user_data, const RdpCaptureProcessInfo* process_info, RdpCaptureGpuApi api);
    void* user_data;  ///< The user data to call with the callback.
} RdpCaptureAppFilter;

typedef enum
{
    kRdpCaptureLogLevelVerbose = 0,
    kRdpCaptureLogLevelInfo    = 1,
    kRdpCaptureLogLevelWarning = 2,
    kRdpCaptureLogLevelError   = 4
} RdpCaptureLogLevel;

/// @brief Callback that gets called when the API implementation logs a message.
typedef struct RdpCaptureLogCallback
{
    /// @brief Called when a message is logged.
    ///
    /// Multiple threads may call this callback at the same time.
    /// @param [in] user_data The user data passed to the callback.
    /// @param [in] level The logging level of the message.
    /// @param [in] source The source of the logging message.
    /// @param [in] process_id The process that the log message is associated with, 0 if there is no association.
    /// @param [in] msg The logged message.
    void (*log)(void* user_data, RdpCaptureLogLevel level, const char* source, uint32_t process_id, const char* msg);
    void* user_data;  ///< The user data to call with the callback.
} RdpCaptureLogCallback;

/// @brief Optional remote connection info for connecting to a non-local router.
typedef struct RdpCaptureRemoteConnectionInfo
{
    const char* hostname;  ///< Remote hostname or IP address. Null or empty for local.
    uint16_t    port;      ///< Remote port. Use 0 to let the tool default the port.
} RdpCaptureRemoteConnectionInfo;

/// @brief RDP Capture context initialization params.
typedef struct RdpCaptureContextInitParams
{
    RdpCaptureAppFilter   app_filter;    ///< The filter callback to decide which applications connect.
    RdpCaptureLogCallback log_callback;  ///< Callback for logging messages.

    RdpCaptureRemoteConnectionInfo remote_connection;  ///< Optional remote connection settings.

} RdpCaptureContextInitParams;

/// @brief Initializes the capture context.
///
/// Only one context is allowed per system. Both Radeon Developer Panel and Radeon Developer Service create contexts, so this function will fail if
/// either are open. If the context should connect to the current process, this function should be called before the first device is initialized.
///
/// @param [in] init_params The initialization parameters.
/// @param [out] out_context A newly initialized RdpCaptureContext context.
/// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not. If this function fails, out_handle will be unchanged.
typedef RdpCaptureResult (*RdpCaptureFnInitialize)(const RdpCaptureContextInitParams* init_params, RdpCaptureContext* out_context);

/// @brief Enables a feature in a capture context.
///
/// In order to enable a feature, the context must not have a connected process.
/// If a process was previously connected, but no longer is, features can again be enabled.
/// @param [in] context The context to enable the feature in.
/// @param [in] feature_params The parameters to enable the feature with.
/// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not.
typedef RdpCaptureResult (*RdpCaptureFnEnableFeature)(RdpCaptureContext context, const RdpCaptureFeatureEnableParams* feature_params);

/// @brief Disables a feature in the capture context.
///
/// In order to disable a feature, the context must not have a connected process.
/// If a process was previously connected, but no longer is, features can again be disabled.
/// @param [in] context The context to disable the feature in.
/// @param [in] feature The feature to disable.
/// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code if not.
typedef RdpCaptureResult (*RdpCaptureFnDisableFeature)(RdpCaptureContext context, RdpCaptureFeature feature);

/// @brief Destroys a capture context.
///
/// This should be called before the process exits or the system might end up in a bad state, however features do not need to be disabled.
/// @param context The context to destroy.
typedef void (*RdpCaptureFnDestroy)(RdpCaptureContext context);

/// @brief Frees a memory allocation that was made by an RDP capture API call.
/// @param [in] ptr The memory to free.
typedef void (*RdpCaptureFnFree)(void* ptr);

/// @brief Gets information about the connected process.
///
/// A context can only connect to a single process at a time.
/// @param [in] context The context to get connected process info for.
/// @param [out] out_process_info The information about the connected process.
typedef void (*RdpCaptureFnGetProcessInfo)(RdpCaptureContext context, RdpCaptureProcessInfo* out_process_info);

/// @brief Gets the current stage of the feature.
///
/// timeout_ms = 0 means to only poll for the stage and return immediately.
/// timeout_ms > 0 means to wait at most given time (in milliseconds) if the stage is kRdpCaptureFeatureStageCapturing.
/// timeout_ms = UINT64_MAX means to wait indefinitely.
/// @param [in] context The context to get the feature stage for.
/// @param [in] feature The feature to get the stage for.
/// @param [in] timeout_ms The time to wait for the feature to transition away from kRdpCaptureFeatureStageCapturing.
/// @return The current stage of the feature after waiting the timeout.
typedef RdpCaptureFeatureStage (*RdpCaptureFnGetFeatureStage)(RdpCaptureContext context, RdpCaptureFeature feature, uint64_t timeout_ms);

/// @brief Gets the current API connections for the given feature.
///
/// A RdpCaptureApiConnection is created once a feature successfully performs initialization for a connection accepted through the app filter.
/// If initialization fails or the feature does not support the API of the connection, a RdpCaptureApiConnection will not be created.
/// @param [in] context The context to get the API connections for the given feature.
/// @param [in] feature The feature to get the API connections for.
/// @param [out] connections The current connections for the feature (should be freed with RdpCaptureFnFree()).
/// @param [out] num_connections The number of current connections in the @param connections array.
typedef void (*RdpCaptureFnGetApiConnections)(RdpCaptureContext         context,
                                              RdpCaptureFeature         feature,
                                              RdpCaptureApiConnection** connections,
                                              uint64_t*                 num_connections);

/// @brief Sets the description that will be saved in traces captured by the feature.
///
/// This should be called before a feature enters the kRdpCaptureFeatureStageBusy stage.
/// @param [in] context The context to set a feature's trace description for.
/// @param [in] feature The feature to set the trace description for.
/// @param [in] desc A null-terminated, UTF-8 encoded string (newlines supported) that will be saved with traces taken by the feature.
typedef void (*RdpCaptureFnSetTraceDesc)(RdpCaptureContext context, RdpCaptureFeature feature, const char* desc);

/// @brief Begins a trace.
/// @param [in] context The context to begin a trace for.
/// @param [in] connection The connection to begin a trace for. RDP_CAPTURE_API_FIRST_CONNECTION_ID can be specified to begin tracing on the first connection from RdpCaptureFnGetApiConnections.
/// @return The result of beginning the trace.
typedef RdpCaptureResult (*RdpCaptureFnBeginTrace)(RdpCaptureContext context, RdpCaptureApiConnectionId connection);

/// @brief Aborts a trace.
/// @param [in] context The context to abort a trace for.
/// @return The result of aborting a trace.
typedef RdpCaptureResult (*RdpCaptureFnAbortTrace)(RdpCaptureContext context);

/// @brief Bitmask for RdpCaptureProfilingParams flags.
typedef enum
{
    kRdpCaptureProfilingParamFlagNone                     = 0,  ///< No flags.
    kRdpCaptureProfilingParamFlagEnableInstructionTracing = 1,  ///< Enables instruction tracing.
    kRdpCaptureProfilingParamFlagEnableCounterCollection  = 2   ///< Enables counter collection.
} RdpCaptureProfilingParamFlags;

/// @brief The capture mode for profiling captures.
///
/// Determines whether a capture captures a full frame, a number of draws, or a number of dispatches.
/// When set to kRdpCaptureProfilingCaptureModeDefault, the implementation selects the most appropriate
/// mode for the connected API (e.g. frame for graphics APIs, dispatch for compute APIs).
typedef enum RdpCaptureProfilingCaptureMode
{
    kRdpCaptureProfilingCaptureModeDefault  = 0,  ///< Let the implementation choose the capture mode based on the connected API.
    kRdpCaptureProfilingCaptureModeFrame    = 1,  ///< Capture a full frame.
    kRdpCaptureProfilingCaptureModeDraw     = 2,  ///< Capture a number of draw calls (see render_op_count).
    kRdpCaptureProfilingCaptureModeDispatch = 3,  ///< Capture a number of dispatches (see render_op_count).
} RdpCaptureProfilingCaptureMode;

/// @brief The various buffer sizes available for SQTT.
//
/// If a profile contains truncated data, increasing the SQTT buffer size can help.
typedef enum RdpCaptureSqttBufferSize
{
    kRdpCaptureSqttBufferSizeDefault = 0,  ///< The default SQTT buffer size.
    kRdpCaptureSqttBufferSizeMinimum = 1,  ///< The smallest buffer size allowed for SQTT.
    kRdpCaptureSqttBufferSizeLow     = 2,  ///< A SQTT buffer size that will have a low memory impact.
    kRdpCaptureSqttBufferSizeHigh    = 3,  ///< A SQTT buffer size that will have a high memory impact.
    kRdpCaptureSqttBufferSizeMaximum = 4,  ///< The maximum buffer size allowed for SQTT.
} RdpCaptureSqttBufferSize;

/// @brief Capture parameters for the Profiling feature.
typedef struct RdpCaptureProfilingParams
{
    uint32_t flags;  ///< RdpCaptureProfilingParamFlags.

    /// @brief The size of the SQTT buffer.
    RdpCaptureSqttBufferSize sqtt_buffer_size;

    /// @brief Frame terminator begin tag (Vulkan).
    ///
    /// Should be non-zero if being used.
    uint64_t begin_frame_terminator_tag;

    /// @brief Frame terminator end tag (Vulkan).
    ///
    /// Should be non-zero if being used.
    uint64_t end_frame_terminator_tag;

    /// @brief Frame terminator begin string (null-terminated, D3D12).
    ///
    /// Should be non-null/non-empty if being used.
    const char* begin_frame_terminator_string;

    /// @brief Frame terminator end string (null-terminated, D3D12).
    ///
    /// Should be non-null/non-empty if being used.
    const char* end_frame_terminator_string;

    /// @brief The capture mode to use for manual captures.
    ///
    /// Determines the type of capture (frame, draw, or dispatch). See RdpCaptureProfilingCaptureMode.
    RdpCaptureProfilingCaptureMode capture_mode;

    /// @brief The number of render operations to capture in draw or dispatch mode.
    ///
    /// When capture_mode is kRdpCaptureProfilingCaptureModeDraw, this sets the draw count.
    /// When capture_mode is kRdpCaptureProfilingCaptureModeDispatch, this sets the dispatch count.
    /// Ignored when capture_mode is kRdpCaptureProfilingCaptureModeDefault or kRdpCaptureProfilingCaptureModeFrame.
    uint32_t render_op_count;

} RdpCaptureProfilingParams;

/// @brief Gets the default parameters for the Profiling feature.
/// @param [out] params The default parameters.
typedef void (*RdpCaptureFnGetDefaultProfilingParams)(RdpCaptureProfilingParams* params);

/// @brief Sets the parameters for the Profiling feature.
/// @param [in] context The context to set profiling params for.
/// @param [in] params The parameters to set.
typedef void (*RdpCaptureFnSetProfilingParams)(RdpCaptureContext context, const RdpCaptureProfilingParams* params);

/// @brief Function table for functions specific to the Profiling feature.
typedef struct RdpCaptureFeatureProfilingFnTable
{
    RdpCaptureFnGetDefaultProfilingParams get_default_params;  ///< Gets the default Profiling parameters.
    RdpCaptureFnSetProfilingParams        set_params;          ///< Sets the parameters for the Profiling feature.

    RdpCaptureFnBeginTrace begin_trace;  ///< Begins capturing a profile.
    RdpCaptureFnAbortTrace abort_trace;  ///< Aborts a profiling capture.

} RdpCaptureFeatureProfilingFnTable;

/// @brief Dumps a memory trace.
/// @param [in] context The context to dump a memory trace for.
/// @param [in] connection The connection to dump a trace for. RDP_CAPTURE_API_FIRST_CONNECTION_ID can be specified to dump the first connection from RdpCaptureFnGetApiConnections.
/// @return The result of dumping a trace.
typedef RdpCaptureResult (*RdpCaptureFnDumpTrace)(RdpCaptureContext context, RdpCaptureApiConnectionId connection);

/// @brief Inserts a memory tracing snapshot.
/// @param [in] context The context to add a memory trace snapshot for.
/// @param [in] connection The connection to insert a snapshot for. RDP_CAPTURE_API_FIRST_CONNECTION_ID can be specified to insert a snapshot on the first connection from RdpCaptureFnGetApiConnections.
/// @param [in] snapshot_name The name of the snapshot to add (null-terminated).
/// @return The result of inserting the snapshot.
typedef RdpCaptureResult (*RdpCaptureFnInsertSnapshot)(RdpCaptureContext context, RdpCaptureApiConnectionId connection, const char* snapshot_name);

/// @brief Function table for functions specific to the Memory Trace feature.
typedef struct RdpCaptureFeatureMemoryTraceFnTable
{
    RdpCaptureFnInsertSnapshot insert_snapshot;  ///< Inserts a memory tracing snapshot.
    RdpCaptureFnDumpTrace      dump_trace;       ///< Dumps a memory trace.
    RdpCaptureFnAbortTrace     abort_trace;      ///< Aborts a memory trace.
} RdpCaptureFeatureMemoryTraceFnTable;

/// @brief The various buffer sizes available for ray history.
typedef enum RdpCaptureRayHistoryBufferSize
{
    kRdpCaptureRayHistoryBufferSizeRayHistoryDisabled = -1,  ///< Disables ray history
    kRdpCaptureRayHistoryBufferSizeMinimum            = 0,   ///< The smallest buffer size allowed for ray history.
    kRdpCaptureRayHistoryBufferSizeLow                = 1,   ///< A ray history buffer size that will have a low memory impact.
    kRdpCaptureRayHistoryBufferSizeDefault            = 2,   ///< The default ray history buffer size.
    kRdpCaptureRayHistoryBufferSizeHigh               = 3,   ///< A ray history buffer size that will have a high memory impact.
    kRdpCaptureRayHistoryBufferSizeMaximum            = 4,   ///< The maximum buffer size allowed for ray history.
} RdpCaptureRayHistoryBufferSize;

/// @brief Capture parameters for the Raytracing feature.
typedef struct RdpCaptureRaytracingParams
{
    RdpCaptureRayHistoryBufferSize ray_history_buffer_size;  ///< The size of the ray history buffer.

    uint32_t    enable_marker_capture;  ///< Non-zero to enable marker-based capture instead of frame-based.
    const char* marker_begin_string;    ///< The marker string that starts the capture (only used if marker capture is enabled).
    const char* marker_end_string;      ///< The marker string that ends the capture (only used if marker capture is enabled).

} RdpCaptureRaytracingParams;

/// @brief Gets the default parameters for the Profiling feature.
/// @param [out] params The default parameters.
typedef void (*RdpCaptureFnGetDefaultRaytracingParams)(RdpCaptureRaytracingParams* params);

/// @brief Sets the parameters for the Raytracing feature.
/// @param [in] context The context to set raytracing params for.
/// @param [in] params The parameters to set.
typedef void (*RdpCaptureFnSetRaytracingParams)(RdpCaptureContext context, const RdpCaptureRaytracingParams* params);

/// @brief Function table for functions specific to the Raytracing feature.
typedef struct RdpCaptureFeatureRaytracingFnTable
{
    RdpCaptureFnGetDefaultRaytracingParams get_default_params;  ///< Gets the default Raytracing parameters.
    RdpCaptureFnSetRaytracingParams        set_params;          ///< Sets the parameters for the Raytracing feature.

    RdpCaptureFnBeginTrace begin_trace;  ///< Begins capturing a raytracing scene.
    RdpCaptureFnAbortTrace abort_trace;  ///< Aborts a raytracing capture.

} RdpCaptureFeatureRaytracingFnTable;

/// @brief Function table for functions specific to the Crash Analysis feature.
typedef struct RdpCaptureFeatureCrashAnalysisFnTable
{
    RdpCaptureFnAbortTrace abort_trace;  ///< Aborts a crash analysis.
} RdpCaptureFeatureCrashAnalysisFnTable;

/// @brief Size in bytes of a GPU locally unique identifier (LUID).
#define RDP_CAPTURE_GPU_LUID_SIZE 8

/// @brief Locally unique identifiers for GPUs.
typedef uint8_t RdpCaptureGpuLuid[RDP_CAPTURE_GPU_LUID_SIZE];

/// @brief String field sizes used in system info structs.
#define RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE 32   ///< Short string fields (e.g. date, slot ID).
#define RDP_CAPTURE_SYSTEM_STRING_MEDIUM_SIZE 64  ///< Medium string fields (e.g. version, memory type).
#define RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE 128   ///< Long string fields (e.g. name, hostname).
#define RDP_CAPTURE_SYSTEM_STRING_XLONG_SIZE 256  ///< Extra-long string fields (e.g. description).

/// @brief Size of the reserved ABI padding buffer appended to system info structs.
/// Allows new fields to be added in future API patch versions without breaking
/// binary compatibility with existing callers.
#define RDP_CAPTURE_SYSTEM_RESERVED_SIZE 256

/// @brief Operating system memory info.
typedef struct RdpCaptureSystemMemory
{
    char     type[RDP_CAPTURE_SYSTEM_STRING_MEDIUM_SIZE];  ///< Memory type name (e.g. "DDR4"). Empty if unavailable.
    uint64_t physical;                                     ///< Total physical memory size in bytes.
    uint64_t swap;                                         ///< Total swap memory size in bytes.
} RdpCaptureSystemMemory;

/// @brief Operating system info on the connected system.
typedef struct RdpCaptureSystemOs
{
    char                   name[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];          ///< OS name (e.g. "Windows 11 Pro").
    char                   description[RDP_CAPTURE_SYSTEM_STRING_XLONG_SIZE];  ///< OS description string.
    char                   hostname[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];      ///< System hostname.
    RdpCaptureSystemMemory memory;                                             ///< System memory info.
    bool                   etw_supported;                                      ///< True if Event Tracing for Windows (ETW) is available on this OS.
    bool                   etw_has_permission;                                 ///< True if the current user has permission to open ETW sessions.
    bool                   etw_needs_script;                                   ///< True if AddUserToGroup.bat must be run as Administrator to enable ETW.
    uint8_t                reserved[RDP_CAPTURE_SYSTEM_RESERVED_SIZE];         ///< Reserved for future fields; do not use.
} RdpCaptureSystemOs;

/// @brief Driver software info on the connected system.
typedef struct RdpCaptureSystemDriver
{
    char     name[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];                 ///< Driver name.
    char     description[RDP_CAPTURE_SYSTEM_STRING_XLONG_SIZE];         ///< Driver description.
    char     packaging_version[RDP_CAPTURE_SYSTEM_STRING_MEDIUM_SIZE];  ///< Driver packaging version string.
    char     packaging_date[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];      ///< Driver packaging date (YYMMDD). Empty if not available.
    char     software_version[RDP_CAPTURE_SYSTEM_STRING_MEDIUM_SIZE];   ///< Driver software version string (Windows-specific).
    uint32_t packaging_version_major;                                   ///< Driver packaging major version.
    uint32_t packaging_version_minor;                                   ///< Driver packaging minor version.
} RdpCaptureSystemDriver;

/// @brief CPU info for the system.
typedef struct RdpCaptureSystemCpu
{
    char     name[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];             ///< CPU name.
    char     architecture[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];    ///< CPU architecture.
    char     vendor_id[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];       ///< CPU vendor id (e.g. "AuthenticAMD").
    char     cpu_id[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];           ///< CPU identifier string.
    char     device_id[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];       ///< CPU slot identifier (e.g. "CPU0").
    char     virtualization[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];  ///< Virtualization firmware state. Empty if unavailable.
    uint32_t num_physical_cores;                                    ///< Physical core count.
    uint32_t num_logical_cores;                                     ///< Logical core count.
    uint32_t max_clock_speed_mhz;                                   ///< Maximum CPU clock speed in MHz.
    uint64_t timestamp_clock_freq_hz;                               ///< CPU timestamp clock frequency in Hz.
} RdpCaptureSystemCpu;

/// @brief A single GPU memory heap.
typedef struct RdpCaptureGpuMemoryHeap
{
    char     heap_type[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];  ///< Heap type (typically "Local" or "Invisible").
    uint64_t physical_address;                                 ///< Physical heap location as a byte offset.
    uint64_t size;                                             ///< Physical heap size in bytes.
} RdpCaptureGpuMemoryHeap;

/// @brief Detailed GPU memory information.
typedef struct RdpCaptureGpuMemoryDetails
{
    char                     type[RDP_CAPTURE_SYSTEM_STRING_SHORT_SIZE];  ///< GPU memory type string.
    uint32_t                 mem_ops_per_clock;                           ///< Memory operations per clock.
    uint32_t                 bus_bit_width;                               ///< Memory bus width in bits.
    uint64_t                 bandwidth;                                   ///< Computed memory bandwidth in bytes/second.
    uint64_t                 mem_clock_min_hz;                            ///< Minimum memory clock in Hz.
    uint64_t                 mem_clock_max_hz;                            ///< Maximum memory clock in Hz.
    RdpCaptureGpuMemoryHeap* heaps;                                       ///< Array of memory heaps. May be NULL when num_heaps is 0.
    uint64_t                 num_heaps;                                   ///< Number of entries in @p heaps.
} RdpCaptureGpuMemoryDetails;

/// @brief PCI bus location for a GPU.
typedef struct RdpCaptureGpuPciInfo
{
    uint32_t bus;        ///< PCI bus number.
    uint32_t device;     ///< PCI device number.
    uint32_t function;   ///< PCI function number.
    uint32_t packed_id;  ///< Bus/device/function fields packed as a 32-bit identifier.
} RdpCaptureGpuPciInfo;

/// @brief ASIC identification info for a GPU.
typedef struct RdpCaptureGpuAsicInfo
{
    uint64_t engine_clock_min_hz;  ///< Minimum shader engine clock in Hz.
    uint64_t engine_clock_max_hz;  ///< Maximum shader engine clock in Hz.
    uint64_t gpu_counter_freq_hz;  ///< GPU timestamp counter frequency in Hz.
    uint32_t family;               ///< Hardware family ID.
    uint32_t device_id;            ///< PCI device ID.
    uint32_t revision;             ///< PCI revision ID.
    uint32_t e_rev;                ///< Hardware revision ID (e_rev).
} RdpCaptureGpuAsicInfo;

/// @brief 'Big Software' release version triplet.
typedef struct RdpCaptureGpuBigSw
{
    uint32_t major;  ///< Major version.
    uint32_t minor;  ///< Minor version.
    uint32_t misc;   ///< Subminor/misc/patch version.
} RdpCaptureGpuBigSw;

/// @brief Detailed info about a GPU on the system.
typedef struct RdpCaptureGpu
{
    RdpCaptureGpuLuid          luid;                                       ///< Locally unique identifier for the GPU.
    char                       name[RDP_CAPTURE_SYSTEM_STRING_LONG_SIZE];  ///< GPU name.
    uint32_t                   vendor_id;                                  ///< PCI vendor id.
    uint32_t                   subsystem_id;                               ///< PCI subsystem id.
    RdpCaptureGpuAsicInfo      asic;                                       ///< ASIC info (includes PCI device id and revision id).
    RdpCaptureGpuMemoryDetails memory;                                     ///< Memory info (includes per-heap details).
    RdpCaptureGpuPciInfo       pci;                                        ///< PCI info (bus/device/function).
    RdpCaptureGpuBigSw         big_sw;                                     ///< Big Software version info.
    int32_t                    driver_index;  ///< Index into RdpCaptureSystemInfo::drivers identifying the driver loaded for this GPU, or -1 if no
                                              ///< per-device mapping is available. May be -1 even when num_drivers > 0 (e.g. when the drivers array
                                              ///< was synthesized from the system-wide record).
} RdpCaptureGpu;

/// @brief Aggregated system info matching the RDP UI's System Info panel.
///
/// Populated by RdpCaptureFnGetSystemInfo and freed by RdpCaptureFnFreeSystemInfo.
typedef struct RdpCaptureSystemInfo
{
    RdpCaptureSystemOs     os;         ///< Operating system info.
    RdpCaptureSystemDriver driver;     ///< System-wide driver record (legacy global entry: Adrenalin packaging info on Windows, dpkg/pacman/dnf record
                                       ///< on Linux). Used as a fallback when a GPU has no per-device driver mapping. May be zero-initialized when no
                                       ///< global record is available.
    RdpCaptureSystemDriver* drivers;   ///< Array of driver records detected on the system; entries may be synthesized from the system-wide @p driver
                                       ///< record when no per-device data is available. May be NULL when num_drivers is 0. A GPU's @p driver_index
                                       ///< references an entry only when >= 0.
    uint64_t             num_drivers;  ///< Number of entries in @p drivers.
    RdpCaptureSystemCpu* cpus;         ///< Array of CPUs on the system. May be NULL when num_cpus is 0.
    uint64_t             num_cpus;     ///< Number of entries in @p cpus.
    RdpCaptureGpu*       gpus;         ///< Array of GPUs on the system. May be NULL when num_gpus is 0.
    uint64_t             num_gpus;     ///< Number of entries in @p gpus.
} RdpCaptureSystemInfo;

/// @brief Gets aggregated system info (OS, driver, CPUs, GPUs) for the connected system.
///
/// This is the public entry point that mirrors the RDP UI's System Info panel.
/// On success the caller owns the nested allocations (drivers, CPUs, GPUs and per-GPU
/// memory heaps) and must release them with RdpCaptureFnFreeSystemInfo().
/// @param [in] context The context to query.
/// @param [out] out_info The system info to populate. On error this struct is unmodified.
/// @return kRdpCaptureResultSuccess if successful, or a RdpCaptureResult error code otherwise.
typedef RdpCaptureResult (*RdpCaptureFnGetSystemInfo)(RdpCaptureContext context, RdpCaptureSystemInfo* out_info);

/// @brief Frees the nested allocations of a struct previously populated by RdpCaptureFnGetSystemInfo.
///
/// After this call the struct's pointer fields are set to NULL and counts are zeroed.
/// May be called with a NULL @p info (no-op).
/// @param [in,out] info The system info struct to release.
typedef void (*RdpCaptureFnFreeSystemInfo)(RdpCaptureSystemInfo* info);

/// @brief The different clock mode ids for a GPU.
typedef enum RdpCaptureGpuClockMode
{
    kRdpCaptureGpuClocksModeUnknown = 0,  ///< An unknown clock mode.
    kRdpCaptureGpuClocksModeNormal  = 1,  ///< Default clocking behavior where the GPU will automatically clock up and down.
    kRdpCaptureGpuClocksModeStable  = 2,  ///< Gpu will keep clocks stable at a specific, thermally stable speed.
    kRdpCaptureGpuClocksModePeak    = 3   ///< Gpu will keep clocks stable at their peak. This mode cannot be manually set.
} RdpCaptureGpuClockMode;

/// @brief Details about a clock mode that a GPU supports.
typedef struct RdpCaptureGpuClockModeDetails
{
    RdpCaptureGpuClockMode mode;  ///< The clock mode.

    uint64_t min_gpu_freq;  ///< GPU minimum shader clock frequency in hertz.
    uint64_t max_gpu_freq;  ///< GPU maximum shader clock frequency in hertz.

    uint64_t min_mem_freq;  ///< GPU minimum memory clock frequency in hertz.
    uint64_t max_mem_freq;  ///< GPU maximum memory clock frequency in hertz.

} RdpCaptureGpuClockModeDetails;

/// @brief Gets the details of the supported clock modes for the given GPU.
/// @param [in] context The context to get the supported clock mode details from.
/// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to get the supported clock modes for.
/// @param [out] modes The supported clock modes for the GPU (should be freed with RdpCaptureFnFree()).
/// @param [out] num_modes The number of modes in the @param modes array.
typedef void (*RdpCaptureFnGetGpuClockModes)(RdpCaptureContext context, uint64_t gpu_index, RdpCaptureGpuClockModeDetails** modes, uint64_t* num_modes);

/// @brief Queries the current clock mode of a given GPU.
/// @param [in] context The context to query the GPU current clock mode from.
/// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to query the clock mode for.
/// @param [out] mode The current clock mode for the GPU.
/// @return The result of querying the current clock mode for the GPU.
typedef RdpCaptureResult (*RdpCaptureFnQueryGpuCurrentClockMode)(RdpCaptureContext context, uint64_t gpu_index, RdpCaptureGpuClockMode* mode);

/// @brief Sets the current clock mode of a given GPU.
/// @param [in] context The context to use to set the clock mode of the GPU.
/// @param [in] gpu_index The index of a GPU from a call to RdpCaptureFnGetSystemInfo() to set the clock mode for.
/// @param [in] mode The desired clock mode for the GPU.
typedef RdpCaptureResult (*RdpCaptureFnSetCurrentGpuClockMode)(RdpCaptureContext context, uint64_t gpu_index, RdpCaptureGpuClockMode mode);

/// @brief Adds a process name pattern to the blocklist.
///
/// The pattern is matched against the process filename (basename), not the full path.
/// Supported pattern syntax includes:
///  - '*' to match any sequence of characters.
///  - '?' to match exactly one character.
///  - Character classes such as '[abc]'.
///  - Negated character classes such as '[!abc]'.
///  - Character ranges within classes such as '[a-z]'.
///  - Backslash escaping to match special characters literally.
/// Has no effect if the pattern is already present.
/// @param [in] context The capture context.
/// @param [in] pattern The pattern to block (e.g. "svchost.exe", "MyApp*", "Game[0-9].exe").
/// @return kRdpCaptureResultSuccess, or kRdpCaptureResultInvalidParams if pattern is null/empty.
typedef RdpCaptureResult (*RdpCaptureFnBlocklistAddEntry)(RdpCaptureContext context, const char* pattern);

/// @brief Removes a process name pattern from the blocklist.
/// @param [in] context The capture context.
/// @param [in] pattern The pattern to remove.
/// @return kRdpCaptureResultSuccess if removed, kRdpCaptureResultNotFound if not present,
///         or kRdpCaptureResultInvalidParams if pattern is null.
typedef RdpCaptureResult (*RdpCaptureFnBlocklistRemoveEntry)(RdpCaptureContext context, const char* pattern);

/// @brief Clears all blocklist entries, including built-in defaults.
/// @param [in] context The capture context.
typedef void (*RdpCaptureFnBlocklistClear)(RdpCaptureContext context);

/// @brief Returns all current blocklist patterns.
///
/// The returned array and each string must be freed with RdpCaptureFnBlocklistFreeEntries().
/// @param [in]  context      The capture context.
/// @param [out] entries      Array of null-terminated pattern strings.
/// @param [out] num_entries  Number of entries written.
typedef void (*RdpCaptureFnBlocklistGetEntries)(RdpCaptureContext context, char*** entries, uint64_t* num_entries);

/// @brief Frees memory returned by RdpCaptureFnBlocklistGetEntries().
/// @param [in] entries      The array returned by RdpCaptureFnBlocklistGetEntries().
/// @param [in] num_entries  The number of entries in the array.
typedef void (*RdpCaptureFnBlocklistFreeEntries)(char** entries, uint64_t num_entries);

/// @brief Loads additional blocklist entries from a text file.
///
/// File format: one pattern per line; lines starting with '#' are comments.
/// Duplicate entries are silently ignored.
/// @param [in] context   The capture context.
/// @param [in] file_path Path to the blocklist file.
/// @return kRdpCaptureResultSuccess, kRdpCaptureResultNotFound if the file cannot be opened,
///         or kRdpCaptureResultInvalidParams if file_path is null/empty.
typedef RdpCaptureResult (*RdpCaptureFnBlocklistLoadFile)(RdpCaptureContext context, const char* file_path);

/// @brief Registers a callback invoked whenever a process is blocked by the blocklist.
///
/// Pass nullptr as callback to unregister. The callback receives the full process path and PID.
/// @param [in] context   The capture context.
/// @param [in] callback  Function called with (user_data, full_process_path, process_id), or nullptr.
/// @param [in] user_data Opaque pointer forwarded to the callback.
typedef void (*RdpCaptureFnBlocklistSetBlockedCallback)(RdpCaptureContext context,
                                                        void (*callback)(void* user_data, const char* process_path, uint32_t process_id),
                                                        void* user_data);

/// @brief Function table for the application blocklist.
typedef struct RdpCaptureBlocklistFnTable
{
    RdpCaptureFnBlocklistAddEntry           add_entry;             ///< Adds a process name pattern to the blocklist.
    RdpCaptureFnBlocklistRemoveEntry        remove_entry;          ///< Removes a process name pattern from the blocklist.
    RdpCaptureFnBlocklistClear              clear;                 ///< Clears all blocklist entries (including defaults).
    RdpCaptureFnBlocklistGetEntries         get_entries;           ///< Gets all current blocklist patterns.
    RdpCaptureFnBlocklistFreeEntries        free_entries;          ///< Frees entries returned by get_entries.
    RdpCaptureFnBlocklistLoadFile           load_file;             ///< Loads additional entries from a text file.
    RdpCaptureFnBlocklistSetBlockedCallback set_blocked_callback;  ///< Registers a callback for when a process is blocked.
} RdpCaptureBlocklistFnTable;

/// @brief Generic function-pointer type used to reserve ABI space for function tables that are
/// compiled out in some build configurations, keeping RdpCaptureFnTable layout identical across builds.
typedef void (*RdpCaptureFnReserved)(void);

typedef struct RdpCaptureFnTable
{
    RdpCaptureFnInitialize     initialize;       ///< Initializes a capture API context.
    RdpCaptureFnEnableFeature  enable_feature;   ///< Enables a feature.
    RdpCaptureFnDisableFeature disable_feature;  ///< Disables a feature.
    RdpCaptureFnDestroy        destroy;          ///< Destroys a capture API context.
    RdpCaptureFnFree           free;             ///< Frees memory allocated by an RDP capture API call.

    RdpCaptureFnGetProcessInfo get_process_info;  ///< Gets information about the connected process.

    RdpCaptureFnGetGpuClockModes         get_gpu_clock_modes;           ///< Gets the clock modes for the GPU.
    RdpCaptureFnQueryGpuCurrentClockMode query_gpu_current_clock_mode;  ///< Queries the current clock mode of a GPU.
    RdpCaptureFnSetCurrentGpuClockMode   set_gpu_current_clock_mode;    ///< Sets the current clock mode for a GPU.

    RdpCaptureFnGetFeatureStage   get_feature_stage;    ///< Gets the stage of the feature.
    RdpCaptureFnGetApiConnections get_api_connections;  ///< Gets the current API connections for the given feature.
    RdpCaptureFnSetTraceDesc      set_trace_desc;       ///< Sets the trace description.

    RdpCaptureFnGetSystemInfo  get_system_info;   ///< Gets aggregated system info (OS, driver, CPUs, GPUs).
    RdpCaptureFnFreeSystemInfo free_system_info;  ///< Frees nested allocations populated by get_system_info.

    RdpCaptureFeatureProfilingFnTable     profiling;       ///<  The function table for the Profiling feature.
    RdpCaptureFeatureMemoryTraceFnTable   memory_trace;    ///< The function table for the Memory Trace feature.
    RdpCaptureFeatureRaytracingFnTable    raytracing;      ///< The function table for the Raytracing feature.
    RdpCaptureFeatureCrashAnalysisFnTable crash_analysis;  ///< The function table for the Crash Analysis feature.

    RdpCaptureFnReserved       reserved_experiments[5];  ///< ABI placeholder for experiments; must equal sizeof(RdpCaptureExperimentsFnTable).
    RdpCaptureBlocklistFnTable blocklist;                ///< The function table for the application blocklist. Fixed offset across all build configurations.

    RdpCaptureFnReserved reserved_settings[5];  ///< ABI placeholder for settings; must equal sizeof(RdpCaptureSettingsFnTable).

} RdpCaptureFnTable;

// The reserved placeholders above stand in for function-pointer tables, so their element type must be a
// function pointer (data and function pointers can differ in size/alignment on some ABIs). These checks
// keep the real tables in sync with the placeholders so the struct layout stays identical across builds.
// The helper picks C++ static_assert, C11 _Static_assert, or a C99-compatible negative-array fallback, so
// the header still compiles under strict C99 (e.g. the C example target). It is #undef'd afterwards so no
// helper macro leaks into consumers of this public header.
#if defined(__cplusplus)
#define RDP_CAPTURE_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define RDP_CAPTURE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define RDP_CAPTURE_STATIC_ASSERT_CAT2(a, b) a##b
#define RDP_CAPTURE_STATIC_ASSERT_CAT(a, b) RDP_CAPTURE_STATIC_ASSERT_CAT2(a, b)
#define RDP_CAPTURE_STATIC_ASSERT(cond, msg) typedef char RDP_CAPTURE_STATIC_ASSERT_CAT(rdp_capture_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

#undef RDP_CAPTURE_STATIC_ASSERT
#undef RDP_CAPTURE_STATIC_ASSERT_CAT
#undef RDP_CAPTURE_STATIC_ASSERT_CAT2

///@brief The major version of the API.
#define RDP_CAPTURE_API_VERSION_MAJOR 1

/// @brief The minor version of the API.
#define RDP_CAPTURE_API_VERSION_MINOR 0

/// @brief The patch version of the API.
#define RDP_CAPTURE_API_VERSION_PATCH 1

/// @brief Get the function table.
/// @param [in] major_version The desired major version.
/// @param [in] minor_version The desired minor version.
/// @param [in] patch_version The desired patch version.
/// @param [out] api_out Where to write the API table to.
/// @return The result of getting the function table.
typedef RdpCaptureResult (*RdpCaptureFnGetFnTable)(uint32_t major_version, uint32_t minor_version, uint32_t patch_version, RdpCaptureFnTable* api_out);

/// @brief Get the function table.
/// @param [in] major_version The desired major version.
/// @param [in] minor_version The desired minor version.
/// @param [in] patch_version The desired patch version.
/// @param [out] api_out Where to write the API table to.
/// @return The result of getting the function table.
RDP_CAPTURE_API_EXPORT RdpCaptureResult RdpCaptureGetFnTable(uint32_t           major_version,
                                                             uint32_t           minor_version,
                                                             uint32_t           patch_version,
                                                             RdpCaptureFnTable* api_out);

#ifdef __cplusplus
}
#endif

#endif
