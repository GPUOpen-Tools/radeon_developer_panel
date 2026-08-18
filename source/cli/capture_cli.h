// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Capture CLI class definition for RadeonDeveloperPanelCLI.

#ifndef RDP_SOURCE_CLI_CAPTURE_CLI_H_
#define RDP_SOURCE_CLI_CAPTURE_CLI_H_

#include <atomic>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "RdpCaptureApi.h"

/// @brief Capture mode enum.
enum class CaptureMode
{
    kProfiling,
    kRaytracing,
    kMemoryTrace,
    kCrashAnalysis,
    kClocks
};

/// @brief Auto-capture mode for RGP.
enum class AutoCaptureMode
{
    kNone,          ///< No auto-capture, manual trigger required.
    kFrameIndex,    ///< Auto-capture at a specific frame index.
    kDispatchIndex  ///< Auto-capture at specific dispatch indices.
};

/// @brief Configuration for the capture CLI.
struct CaptureConfig
{
    std::string output_path;     ///< Output file path.
    CaptureMode mode;            ///< Capture mode.
    std::string process_filter;  ///< Process name filter (optional).
    bool        verbose;         ///< Enable verbose logging.

    std::string remote_host;  ///< Remote hostname or IP address (optional).
    uint16_t    remote_port;  ///< Remote port (only used when remote_host is set).

    // Auto-capture settings (RGP)
    AutoCaptureMode auto_capture_mode;          ///< Auto-capture mode.
    uint32_t        frame_capture_index;        ///< Frame index for auto-capture (when mode is kFrameIndex).
    uint32_t        dispatch_start_index;       ///< Dispatch start index for auto-capture (when mode is kDispatchIndex).
    uint32_t        dispatch_count;             ///< Number of dispatches to capture (when mode is kDispatchIndex).
    uint32_t        dispatch_capture_delay_ms;  ///< Delay in milliseconds before dispatch auto-capture starts.

    bool                     enable_instruction_tracing;     ///< Enable instruction tracing (RGP).
    bool                     enable_counter_collection;      ///< Enable counter collection (RGP).
    bool                     enable_shader_instrumentation;  ///< Enable shader instrumentation (RGP).
    RdpCaptureSqttBufferSize sqtt_buffer_size;               ///< SQTT buffer size (RGP).
    uint32_t                 rgp_capture_mode    = 0;        ///< RGP capture mode: 0=default, 1=frame, 2=draw, 3=dispatch.
    uint32_t                 rgp_render_op_count = 1;        ///< Render op count for draw/dispatch capture modes (RGP).

    // Raytracing settings (RRA)
    RdpCaptureRayHistoryBufferSize rra_ray_history_buffer_size;  ///< Ray history buffer size (RRA).
    bool        rra_collect_ray_dispatch_data = true;  ///< Collect ray dispatch data (RRA). When false, ray history is disabled regardless of buffer size.
    uint32_t    rra_delay_ms                  = 0;     ///< Delay before triggering each raytracing capture, in milliseconds (RRA). 0 = no delay.
    bool        rra_enable_marker_capture;             ///< Enable marker-based capture instead of frame-based (RRA).
    std::string rra_marker_begin;                      ///< Marker string that starts the capture (RRA).
    std::string rra_marker_end;                        ///< Marker string that ends the capture (RRA).

    bool enable_enhanced_crash_analysis;  ///< Enable enhanced crash analysis (RGD).

    // Crash Analysis (RGD) summary / shader-PDB options.
    bool                     rgd_generate_text_summary = false;   ///< Request a text summary alongside the .rgd file.
    bool                     rgd_generate_json_summary = false;   ///< Request a JSON summary alongside the .rgd file.
    bool                     rgd_show_marker_source    = false;   ///< Display execution-marker source information in the summary.
    bool                     rgd_expand_markers        = false;   ///< Expand all execution-marker nodes in the summary.
    std::vector<std::string> rgd_pdb_search_paths;                ///< DXC shader PDB search paths.
    bool                     rgd_pdb_include_subfolders = false;  ///< Recurse into subfolders when resolving DXC PDBs.
    std::string              rgd_cli_path;  ///< Optional path to the rgd executable (file or directory). Defaults to the current working directory.
    bool                     rgd_collect_wave_sgprs = false;  ///< Collect wave SGPRs during enhanced crash analysis.
    bool                     rgd_collect_wave_vgprs = false;  ///< Collect wave VGPRs during enhanced crash analysis.

    std::vector<std::string> blocklist_patterns;  ///< Additional blocklist patterns (--block).
    std::string              blocklist_file;      ///< Path to a blocklist file to load (--block-file).
    bool                     list_blocklist;      ///< List all active blocklist entries and exit.

    bool system_info;  ///< Print system info (OS, driver, CPUs, GPUs) and exit.

    RdpCaptureGpuClockMode clock_mode;  ///< Clock mode to set (kClocks mode).
    uint64_t               gpu_index;   ///< GPU index to target (kClocks mode).
};

/// @brief The CaptureCli class handles GPU capture operations.
class CaptureCli
{
public:
    /// @brief Constructor.
    /// @param config The capture configuration.
    explicit CaptureCli(CaptureConfig config);

    /// @brief Destructor.
    ~CaptureCli();

    /// @brief Runs the capture CLI.
    /// @return The exit code.
    int Run();

    /// @brief Interrupt the capture and cleanup.
    /// Call this before exiting due to signals.
    void Interrupt();

    // Public callback methods (called from C callback wrappers)

    /// @brief App filter callback.
    /// @param process_info Process information.
    /// @param api The GPU API.
    /// @return True if the process should be connected.
    bool AppFilter(const RdpCaptureProcessInfo* process_info, RdpCaptureGpuApi api);

    /// @brief Log callback.
    /// @param level Log level.
    /// @param source Source of the log message.
    /// @param process_id Process ID.
    /// @param msg Log message.
    static void LogCallback(RdpCaptureLogLevel level, const char* source, uint32_t process_id, const char* msg);

    /// @brief Trace finished callback.
    /// @param feature The feature that finished.
    /// @param connection The connection ID.
    /// @param result The result of the trace.
    /// @param size Size of the trace data.
    /// @param data The trace data.
    void TraceFinished(RdpCaptureFeature feature, RdpCaptureApiConnectionId connection, RdpCaptureResult result, std::span<const uint8_t> data);

    /// @brief Status change callback.
    /// @param feature The feature whose status changed.
    /// @param new_stage The new detailed stage.
    /// @param old_stage The previous detailed stage.
    void StatusChanged(RdpCaptureFeature feature, RdpCaptureDetailedStage new_stage, RdpCaptureDetailedStage old_stage);

    /// @brief Progress update callback.
    /// @param info The progress information.
    void ProgressUpdated(const RdpCaptureProgressInfo* info);

private:
    /// @brief Initializes the capture API.
    /// @return True if successful.
    bool Initialize();

    /// @brief Enables the capture feature based on configuration.
    /// @return True if successful.
    bool EnableFeature();

    /// @brief Cleanup resources.
    void Cleanup();

    /// @brief Performs a single capture operation.
    /// @return 0 on success, 1 on fatal error, -1 on recoverable error (continue in interactive mode).
    int PerformCapture();

    /// @brief Get the file extension for the current mode.
    /// @return The file extension (including dot).
    std::string GetFileExtension() const;

    /// @brief Generate the output file path.
    /// @return The output file path (with timestamp if not specified by user).
    std::string GenerateOutputPath() const;

    /// @brief Get the feature enum for the current mode.
    /// @return The RdpCaptureFeature enum value.
    RdpCaptureFeature GetFeature() const;

    /// @brief Get the mode name for display.
    /// @return The mode name string.
    std::string GetModeName() const;

    /// @brief Applies blocklist entries and file from the configuration.
    void ApplyBlocklist() const;

    /// @brief Lists all active blocklist entries to stdout and exits.
    void ListBlocklist() const;

    /// @brief Prints system info (OS, driver, CPUs, and detailed GPU info) to stdout.
    void PrintSystemInfo() const;

    /// @brief Prints a warning to stderr if ETW is not configured (AddUserToGroup.bat has not been run).
    /// On Windows, ETW is required for DX12 Signal/Wait capture and memory trace resource naming.
    void WarnIfEtwNotConfigured() const;

    /// @brief Warns to stderr if the RRA trace contains no acceleration structures.
    /// Mirrors the "No acceleration structures captured" warning shown in the RDP GUI.
    /// @return True if no acceleration structures were found, false otherwise.
    bool WarnIfNoAccelerationStructures() const;

    /// @brief Runs the clocks mode (query or set GPU clock modes).
    /// @return The exit code.
    int RunClocksMode() const;

    /// @brief Get the name of a clock mode for display.
    /// @param mode The clock mode.
    /// @return The clock mode name string.
    static std::string GetClockModeName(RdpCaptureGpuClockMode mode);

    /// @brief Waits for a GPU crash to occur and saves the dump.
    /// @return The exit code.
    int WaitForCrashAndSave();

    /// @brief Generates text and/or JSON crash summaries by spawning the rgd executable.
    /// @param dump_path Path of the .rgd dump file to summarize.
    void GenerateCrashSummaries(const std::string& dump_path) const;

    /// @brief Waits for auto-capture to complete and saves the trace.
    /// @return The exit code.
    int WaitForAutoCaptureAndSave();

    /// @brief Runs the manual capture interactive loop.
    /// @return The exit code.
    int RunManualCaptureLoop();

    /// @brief Waits for the feature to reach a capturable state.
    /// @return 0 if ready, 1 on error or interrupt.
    int WaitForFeatureReady();

    /// @brief Configures auto-capture parameters for profiling mode.
    /// @param [out] enable_params The feature enable parameters to configure.
    void ConfigureProfilingAutoCapture(RdpCaptureFeatureEnableParams& enable_params) const;

    /// @brief Sets additional profiling parameters after feature enable.
    void ApplyProfilingParams() const;

    /// @brief Sets additional raytracing parameters after feature enable.
    void ApplyRaytracingParams() const;

    /// @brief Saves captured trace data and handles cleanup/exit logic.
    void SaveAndCleanupCapture();

    /// @brief Renders a progress bar to the terminal.
    /// @param progress A value in [0.0, 1.0].
    /// @param label Optional label to display after the bar. May be empty.
    static void RenderProgressBar(float progress, const std::string& label);

    /// @brief Gets a human-readable string for a detailed stage.
    /// @param stage The detailed stage.
    /// @return A string describing the stage.
    static std::string GetDetailedStageName(RdpCaptureDetailedStage stage);

    /// @brief Clears the current progress bar line from the terminal.
    static void ClearProgressLine();

    CaptureConfig        config_;                  ///< Capture configuration.
    RdpCaptureFnTable    fn_table_{};              ///< Function table.
    RdpCaptureContext    context_;                 ///< Capture context.
    std::atomic_bool     capture_complete_;        ///< Flag indicating capture is complete.
    std::atomic_bool     interrupted_;             ///< Flag indicating user interrupted.
    std::vector<uint8_t> trace_data_;              ///< Captured trace data.
    bool                 capture_success_;         ///< Flag indicating capture success.
    bool                 capture_requested_;       ///< Flag indicating user explicitly requested a capture via 'c'.
    std::string          connected_process_name_;  ///< Name of the connected process (set by AppFilter).
    bool                 progress_active_;         ///< Flag indicating progress bar is being displayed (guarded by g_output_mutex).
};

/// @brief Parse command line arguments and create capture configuration.
/// @param argc Argument count.
/// @param argv Argument values.
/// @param config Output configuration.
/// @return True if parsing was successful.
bool ParseCommandLine(int argc, char** argv, CaptureConfig& config);

#endif
