// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A wrapper class for using the RdpCaptureAPi to setup and capture
///         RGP profiles and RMV traces.

#include "capture_api_wrapper.h"

#include <stdio.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <dlfcn.h>
#include <string.h>
#define MAX_PATH (260)
#endif

#include <RdpCaptureApi.h>
#include "timer.h"

#define CAPTURE_UNUSED(x) (void)(x);

// Command line option strings
static const char* kOptionRmvEnable                    = "--rmv-enable";
static const char* kOptionRgpEnable                    = "--rgp-enable";
static const char* kOptionRraEnable                    = "--rra-enable";
static const char* kOptionRgpEnableInstructionTracing  = "--rgp-enable-instruction-tracing";
static const char* kOptionRgpEnableCounters            = "--rgp-enable-counters";
static const char* kOptionRgpCaptureCount              = "--rgp-capture-count=";
static const char* kOptionRgpCaptureStartFrame         = "--rgp-capture-start-frame=";
static const char* kOptionRgpCaptureStartDispatchIndex = "--rgp-capture-start-dispatch-index=";
static const char* kOptionRgpCaptureDispatchCount      = "--rgp-capture-dispatch-count=";
static const char* kOptionRgpTimeBetweenProfiles       = "--rgp-time-between-profiles=";
static const char* kOptionRgpCaptureFileName           = "--rgp-capture-file-name=";
static const char* kOptionRmvSnapshotTime              = "--rmv-snapshot-time=";
static const char* kOptionRmvSnapshotName              = "--rmv-snapshot-name=";
static const char* kOptionRmvTraceLength               = "--rmv-trace-length=";
static const char* kOptionRmvTraceFileName             = "--rmv-trace-file-name=";
static const char* kOptionRraCaptureStartFrame         = "--rra-capture-start-frame=";
static const char* kOptionRraCaptureFileName           = "--rra-capture-file-name=";
static const char* kOptionRraEnableRayHistory          = "--rra-enable-ray-history";
static const char* kOptionRraRayHistorySize            = "--rra-history-buffer-size=";
static const char* kOptionRraEnableMarkerCapture       = "--rra-enable-marker-capture";
static const char* kOptionRraMarkerBegin               = "--rra-marker-begin=";
static const char* kOptionRraMarkerEnd                 = "--rra-marker-end=";
static const char* kOptionNumberOfFrames               = "--number-of-frames=";
static const char* kOptionDumpConfig                   = "--dump-config";

static const char* kOptionLogToFile   = "--log-to-file";
static const char* kOptionLogFileName = "--log-file-name=";

static constexpr uint32_t kWaitFinishedTimeoutMs = 30000;

// When using RGP and RMV at the same time, it might be a good idea to make sure an RGP
// trace doesn't happen at the same time as an RMV snapshot is taken since the driver
// allocates memory for the RGP capture and this will end up in the snapshot.

/// @brief A structure containing the capture parameters.
struct CaptureConfig
{
    bool     rgp_enabled;                           ///< Is RGP capture enabled.
    bool     rmv_enabled;                           ///< Is RMV capture enabled.
    bool     rra_enabled;                           ///< Is RRA capture enabled.
    int32_t  rgp_capture_count;                     ///< The number of RGP profiles to capture.
    int32_t  rgp_capture_start_frame;               ///< The frame number on which to take the first RGP capture.
    int32_t  rgp_time_between_profiles;             ///< The time between RGP profiles, in milliseconds.
    bool     rgp_instruction_tracing_enabled;       ///< Is RGP instruction timing enabled.
    bool     rgp_counters_enabled;                  ///< Is RGP counter collection enabled.
    int32_t  rgp_shader_mask;                       ///< The RGP shader mask for instruction timing.
    int32_t  rgp_spm_buffer_size;                   ///< The RGP SPM buffer size.
    bool     rgp_capture_file_name_set;             ///< Flag indicating if the RGP capture file name has been set.
    char     rgp_capture_file_name[MAX_PATH];       ///< The name of the RGP capture file.
    int32_t  rgp_capture_start_dispatch_index = 1;  ///< The dispatch index to start the capture (for OpenCL/HIP).
    int32_t  rgp_capture_dispatch_count;            ///< The number of dispatches to capture (for OpenCL/HIP).
    int32_t  rmv_snapshot_time;                     ///< The timestamp of the RMV snapshot, in milliseconds from the start of the trace.
    char     rmv_snapshot_name[MAX_PATH];           ///< The default RMV snapshot name.
    int32_t  rmv_trace_length;                      ///< How long the RMV trace should be, in milliseconds.
    bool     rmv_trace_file_name_set;               ///< Flag indicating if the RMV trace file name has been set.
    char     rmv_trace_file_name[MAX_PATH];         ///< The name of the RMV trace file.
    int32_t  rra_capture_start_frame;               ///< The frame number on which to request RRA capture.
    bool     rra_capture_file_name_set;             ///< Flag indicating if the RRA capture file name was set.
    char     rra_capture_file_name[MAX_PATH];       ///< The name of the RRA capture file.
    bool     rra_enable_ray_history;                ///< true if ray history should be enabled, false otherwise.
    uint32_t rra_ray_history_buffer_size;           ///< The size index of the ray history buffer. Can be one of: 0=minimum, 1=low, 2=default, 3=high, 4=maximum
    bool     rra_enable_marker_capture;             ///< true if marker-based capture should be used.
    char     rra_marker_begin[256];                 ///< The marker string that starts the capture.
    char     rra_marker_end[256];                   ///< The marker string that ends the capture.
    int32_t  number_of_frames;                      ///< Number of frames to render before exiting. Ignored when RGP or RMV is enabled.

    bool log_to_file;              ///< true if logging should also be output to a file.
    char log_file_name[MAX_PATH];  ///< The optional log file name.

    bool dump_config;  ///< true if the config should be dumped to disk at start of the trace.
};
/// @brief The DevDriver wrapper implementation class.
class CaptureWrapperImpl
{
public:
    /// @brief Constructor.
    CaptureWrapperImpl()
        : context_(nullptr)
        , frame_count_(0)
        , rgp_frames_captured_(0)
        , rgp_done_(true)
        , rmv_snapshot_taken_(false)
        , rmv_done_(true)
        , rra_done_(true)
    {
        // Set up some capture defaults.
        config_.rgp_enabled                      = false;
        config_.rmv_enabled                      = false;
        config_.rra_enabled                      = false;
        config_.rgp_capture_count                = 1;
        config_.rgp_capture_start_frame          = 100;
        config_.rgp_time_between_profiles        = 1000;
        config_.rgp_instruction_tracing_enabled  = false;
        config_.rgp_counters_enabled             = false;
        config_.rgp_shader_mask                  = 1;
        config_.rgp_spm_buffer_size              = -1;
        config_.rgp_capture_file_name_set        = false;
        config_.rgp_capture_start_dispatch_index = -1;
        config_.rgp_capture_dispatch_count       = 0;
        config_.rmv_snapshot_time                = 3500;
        config_.rmv_trace_length                 = 5000;
        config_.rmv_trace_file_name_set          = false;
        config_.rra_capture_start_frame          = 100;
        config_.rra_capture_file_name_set        = false;
        config_.rra_enable_ray_history           = false;
        config_.rra_ray_history_buffer_size      = kRdpCaptureRayHistoryBufferSizeDefault;
        config_.rra_enable_marker_capture        = false;
        config_.number_of_frames                 = 0;
        config_.dump_config                      = false;

        config_.log_to_file = false;

        strcpy(config_.rmv_snapshot_name, "Snapshot 0");
        strcpy(config_.rgp_capture_file_name, "");
        strcpy(config_.rmv_trace_file_name, "");
        strcpy(config_.rra_capture_file_name, "");
        strcpy(config_.rra_marker_begin, "RRABeginMarker");
        strcpy(config_.rra_marker_end, "RRAEndMarker");
        strcpy(config_.log_file_name, "");
    }

    static std::string LogLevelToStr(RdpCaptureLogLevel level)
    {
        switch (level)
        {
        case kRdpCaptureLogLevelVerbose:
            return "VERBOSE";
        case kRdpCaptureLogLevelInfo:
            return "INFO";
        case kRdpCaptureLogLevelWarning:
            return "WARNING";
        case kRdpCaptureLogLevelError:
            return "ERROR";
        default:
            break;
        }

        return "UNKNOWN";
    }

    void Log(const std::string& log_message)
    {
        std::cout << log_message;

        if (log_file_.is_open() && !log_file_.bad())
        {
            log_file_ << log_message;
        }
    }

    static void LogCallback(void* user_data, RdpCaptureLogLevel level, const char* source, uint32_t process_id, const char* msg)
    {
        std::ostringstream stream;
        if (process_id == 0)
        {
            stream << LogLevelToStr(level) << " [" << source << "] " << msg << std::endl;
        }
        else
        {
            stream << LogLevelToStr(level) << " [" << source << " - PID: " << process_id << "] " << msg << std::endl;
        }

        CaptureWrapperImpl* impl = reinterpret_cast<CaptureWrapperImpl*>(user_data);
        impl->Log(stream.str());
    }

    static void RgpTraceFinished(void*                     user_data,
                                 RdpCaptureFeature         feature,
                                 RdpCaptureApiConnectionId conn_id,
                                 RdpCaptureResult          result,
                                 uint64_t                  size,
                                 const uint8_t*            data)
    {
        CAPTURE_UNUSED(feature);
        CAPTURE_UNUSED(conn_id);

        CaptureWrapperImpl* impl = reinterpret_cast<CaptureWrapperImpl*>(user_data);
        const std::string   path = impl->config_.rgp_capture_file_name_set ? impl->config_.rgp_capture_file_name : impl->GenerateTraceFileName(".rgp");

        impl->WriteFileToPath(result, size, data, path);
        impl->rgp_frames_captured_++;

        bool capture_by_dispatch = impl->config_.rgp_capture_start_dispatch_index >= 0 && impl->config_.rgp_capture_dispatch_count > 0;
        impl->rgp_done_          = impl->rgp_frames_captured_ >= impl->config_.rgp_capture_count || capture_by_dispatch;
    }

    static void RmvTraceFinished(void*                     user_data,
                                 RdpCaptureFeature         feature,
                                 RdpCaptureApiConnectionId conn_id,
                                 RdpCaptureResult          result,
                                 uint64_t                  size,
                                 const uint8_t*            data)
    {
        CAPTURE_UNUSED(feature);
        CAPTURE_UNUSED(conn_id);

        CaptureWrapperImpl* impl = reinterpret_cast<CaptureWrapperImpl*>(user_data);
        const std::string   path = impl->config_.rmv_trace_file_name_set ? impl->config_.rmv_trace_file_name : impl->GenerateTraceFileName(".rmv");

        impl->WriteFileToPath(result, size, data, path);

        impl->rmv_trace_triggered_ = false;
        impl->rmv_done_            = true;
    }

    static void RraTraceFinished(void*                     user_data,
                                 RdpCaptureFeature         feature,
                                 RdpCaptureApiConnectionId conn_id,
                                 RdpCaptureResult          result,
                                 uint64_t                  size,
                                 const uint8_t*            data)
    {
        CAPTURE_UNUSED(feature);
        CAPTURE_UNUSED(conn_id);

        CaptureWrapperImpl* impl = reinterpret_cast<CaptureWrapperImpl*>(user_data);
        const std::string   path = impl->config_.rra_capture_file_name_set ? impl->config_.rra_capture_file_name : impl->GenerateTraceFileName(".rra");

        impl->WriteFileToPath(result, size, data, path);

        impl->rra_done_ = true;
    }

    std::string GenerateTimestampString() const
    {
        static constexpr int kBufferSize = 1024;
        char                 time_stamp[kBufferSize];

        time_t t = time(nullptr);
        tm     time{};
        tm*    time_ptr = &time;
#ifdef _WIN32
        localtime_s(&time, &t);
#else
        localtime_r(&t, &time);
#endif

        std::snprintf(time_stamp,
                      kBufferSize,
                      "-%04d%02d%02d-%02d%02d%02d",
                      time_ptr->tm_year + 1900,
                      time_ptr->tm_mon + 1,
                      time_ptr->tm_mday,
                      time_ptr->tm_hour,
                      time_ptr->tm_min,
                      time_ptr->tm_sec);

        return time_stamp;
    }

    std::string GenerateTraceFileName(const std::string& extension) const
    {
        const std::string time_stamp = GenerateTimestampString();

        RdpCaptureProcessInfo process_info{};
        dispatch_table_.get_process_info(context_, &process_info);

        if (process_info.process_id == 0)
        {
            return std::string("Unknown") + time_stamp + extension;
        }

        const std::string filename = GetLastPathComponent(process_info.process_path);
        return RemoveExtension(filename) + time_stamp + extension;
    }

    std::string GetLogfileName() const
    {
        std::string configured_name = config_.log_file_name;
        if (!configured_name.empty())
        {
            return configured_name;
        }

        return "capture-log" + GenerateTimestampString() + ".txt";
    }

    static std::string GetLastPathComponent(const std::string& path)
    {
#ifdef WIN32
        static const std::string separator = "\\";
#else
        static const std::string separator = "/";
#endif

        size_t last_separator = path.find_last_of(separator);
        return last_separator == std::string::npos ? path : path.substr(last_separator + 1);
    }

    static std::string RemoveExtension(const std::string& filename)
    {
        size_t extension_dot = filename.find_last_of(".");
        return extension_dot == std::string::npos ? filename : filename.substr(0, extension_dot);
    }

    void WriteFileToPath(RdpCaptureResult result, uint64_t size, const uint8_t* data, const std::string& path)
    {
        if (result != kRdpCaptureResultSuccess)
        {
            return;
        }

        std::ofstream stream;
        stream.open(path, std::ofstream::out | std::ofstream::binary);

        if (!stream.is_open() || stream.bad())
        {
            return;
        }

        stream.write(reinterpret_cast<const char*>(data), size);
        stream.close();
    }

    /// @brief Destructor.
    ~CaptureWrapperImpl()
    {
        log_file_.close();
    }

    /// @brief Process the command line parameters from the host application.
    ///
    /// @param argc The number of command line arguments.
    /// @param argv The list of command line argument strings.
    void ProcessCommandLine(int argc, char* argv[])
    {
        for (int count = 0; count < argc; count++)
        {
            ProcessCommandLineArgument(argv[count]);
        }

        if (config_.dump_config)
        {
            DumpConfig();
        }
    }

    /// @brief Initialize the DevDriver based on the current configuration settings.
    bool Init()
    {
        log_file_.open(GetLogfileName(), std::ios::out | std::ios::trunc);

        if (!log_file_.is_open() || log_file_.bad())
        {
            log_file_.close();
        }

        // Abort early if everything disabled.
        if (!config_.rgp_enabled && !config_.rmv_enabled && !config_.rra_enabled)
        {
            return true;
        }

#ifndef _WIN32
        RdpCaptureFnGetFnTable RdpCaptureGetFnTable = nullptr;

        void* module = dlopen("libRdpCaptureApi.so", RTLD_NOW);
        if (module != nullptr)
        {
            RdpCaptureGetFnTable = reinterpret_cast<RdpCaptureFnGetFnTable>(dlsym(module, "RdpCaptureGetFnTable"));
        }
        if (RdpCaptureGetFnTable == nullptr)
        {
            context_ = nullptr;
            return false;
        }
#endif
        if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &dispatch_table_) !=
            kRdpCaptureResultSuccess)
        {
            return false;
        }

        rgp_table_ = dispatch_table_.profiling;
        rmv_table_ = dispatch_table_.memory_trace;
        rra_table_ = dispatch_table_.raytracing;

        // In process capture doesn't need a connection callback.
        RdpCaptureContextInitParams params{};
        params.log_callback.log       = LogCallback;
        params.log_callback.user_data = this;

        if (dispatch_table_.initialize(&params, &context_) != kRdpCaptureResultSuccess)
        {
            return false;
        }

        if (!InitFeatures())
        {
            dispatch_table_.destroy(context_);
            return false;
        }

        return true;
    }

    /// @brief Initializes the feature.
    /// @return true if initialization is successful, false otherwise.
    bool InitFeatures()
    {
        RdpCaptureFeatureEnableParams feature_params     = {};
        feature_params.trace_finished_callback.user_data = this;

        if (config_.rgp_enabled)
        {
            feature_params.feature                                = kRdpCaptureFeatureProfiling;
            feature_params.trace_finished_callback.trace_finished = RgpTraceFinished;

            if (config_.rgp_capture_start_dispatch_index != -1)
            {
                feature_params.body.profiling.flags |= kRdpCaptureProfilingEnableParamFlagUseDispatchIndexCapture;
                feature_params.body.profiling.dispatch_start_index = config_.rgp_capture_start_dispatch_index;
                feature_params.body.profiling.dispatch_count       = config_.rgp_capture_dispatch_count;
            }

            if (dispatch_table_.enable_feature(context_, &feature_params) != kRdpCaptureResultSuccess)
            {
                return false;
            }

            // TODO: Do we need the shader mask? devtrace handles it
            RdpCaptureProfilingParams rgp_params{};
            rgp_table_.get_default_params(&rgp_params);

            if (config_.rgp_instruction_tracing_enabled)
            {
                rgp_params.flags |= kRdpCaptureProfilingParamFlagEnableInstructionTracing;
            }

            if (config_.rgp_counters_enabled)
            {
                rgp_params.flags |= kRdpCaptureProfilingParamFlagEnableCounterCollection;
            }

            rgp_table_.set_params(context_, &rgp_params);
            rgp_done_ = false;
        }

        if (config_.rmv_enabled)
        {
            feature_params.feature                                = kRdpCaptureFeatureMemoryTrace;
            feature_params.trace_finished_callback.trace_finished = RmvTraceFinished;

            if (dispatch_table_.enable_feature(context_, &feature_params) != kRdpCaptureResultSuccess)
            {
                return false;
            }

            rmv_done_ = false;
        }

        if (config_.rra_enabled)
        {
            feature_params.feature                                = kRdpCaptureFeatureRaytracing;
            feature_params.trace_finished_callback.trace_finished = RraTraceFinished;

            if (dispatch_table_.enable_feature(context_, &feature_params) != kRdpCaptureResultSuccess)
            {
                return false;
            }

            RdpCaptureRaytracingParams rra_params{};
            rra_table_.get_default_params(&rra_params);

            if (config_.rra_enable_ray_history && (config_.rra_ray_history_buffer_size >= kRdpCaptureRayHistoryBufferSizeMinimum &&
                                                   config_.rra_ray_history_buffer_size <= kRdpCaptureRayHistoryBufferSizeMaximum))
            {
                rra_params.ray_history_buffer_size = static_cast<RdpCaptureRayHistoryBufferSize>(config_.rra_ray_history_buffer_size);
            }

            // Apply marker capture settings
            rra_params.enable_marker_capture = config_.rra_enable_marker_capture ? 1 : 0;
            rra_params.marker_begin_string   = config_.rra_marker_begin;
            rra_params.marker_end_string     = config_.rra_marker_end;

            rra_table_.set_params(context_, &rra_params);
            rra_done_ = false;
        }

        return true;
    }

    /// @brief Function to initialize anything at device initialization time.
    ///
    /// This is the approximate time the RMV trace starts so start the RMV timer.
    void DeviceCreated()
    {
        rmv_timer_.StartTimer();
    }

    /// @brief Close down the DevDriver gracefully.
    void Close()
    {
        if (context_ != nullptr)
        {
            dispatch_table_.destroy(context_);
        }
    }

    /// @brief Called in the host application frame update function to update the RMV and RGP capture state.
    ///
    /// @return true if exit app is required, false if not.
    bool FrameUpdate()
    {
        // Increment frame counter.
        frame_count_++;

        // Early out if no DevDriver functionality required.
        if (config_.rgp_enabled == false && config_.rmv_enabled == false && config_.rra_enabled == false)
        {
            return config_.number_of_frames == 0 ? false : frame_count_ > static_cast<uint64_t>(config_.number_of_frames);
        }

        // Update RGP status if enabled.
        if (config_.rgp_enabled && !rgp_done_)
        {
            UpdateRGPState();
        }

        // Update RMV status if enabled.
        if (config_.rmv_enabled && !rmv_done_)
        {
            UpdateRMVState();
        }

        // Update RRA status if enabled.
        if (config_.rra_enabled && !rra_done_)
        {
            UpdateRRAState();
        }

        // Notify the calling process to terminate if all RGP captures are done and RMV tracing is complete.
        return IsFinished();
    }

    bool PreDispatch()
    {
        if (config_.rgp_enabled)
        {
            UpdateRGPState();
        }

        return rgp_done_;
    }

    bool WaitUntilFinished()
    {
        auto end = std::chrono::system_clock::now() + std::chrono::milliseconds(kWaitFinishedTimeoutMs);
        while (std::chrono::system_clock::now() <= end)
        {
            if (IsFinished())
            {
                return true;
            }

            std::this_thread::yield();
        }

        return false;
    }

private:
    bool IsFinished()
    {
        return (rgp_done_ && rmv_done_ && rra_done_);
    }

public:
    /// @brief Display the command line help paramaters for RMV/RGP capture to std::cout.
    void DisplayCommandLineHelp()
    {
        std::cout << "RMV/RGP/RRA capture command line arguments:" << std::endl;
        std::cout << "--rmv-enable                                  Enable rmv tracing." << std::endl;
        std::cout << "--rgp-enable                                  Enable rgp profiling." << std::endl;
        std::cout << "--rra-enable                                  Enable rra tracing." << std::endl;
        std::cout << "--rgp-enable-instruction-tracing              Enable collection of instruction tracing data with the RGP capture." << std::endl;
        std::cout << "--rgp-capture-count=<number>                  How many RGP profiles to take (default 1)." << std::endl;
        std::cout << "--rgp-capture-start-frame=<number>            Which frame to start taking RGP profiles from (default 100)" << std::endl;
        std::cout << "--rgp-time-between-profiles=<number>          The time between RGP profiles, in milliseconds (default 1000)." << std::endl;
        std::cout << "--rgp-capture-file-name=<string>              The name of the RGP capture file." << std::endl;
        std::cout << "--rgp-capture-start-dispatch-index==<number>  The dispatch index to start the RGP capture from." << std::endl;
        std::cout << "--rgp-capture-dispatch-count==<number>        The number of dispatches to capture (default 10). Only used if the start dispatch index is "
                     "also set."
                  << std::endl;
        std::cout << "--rmv-snapshot-time=<number>                  The time when an RMV snapshot is to be taken, in milliseconds (default 3500)." << std::endl;
        std::cout
            << "--rmv-snapshot-name=<string>                  The RMV snapshot name as a string. If the name contains spaces, the string should be placed in "
            << std::endl;
        std::cout << "--rmv-trace-length=<number>                   The RMV trace length, in milliseconds (default 5000)." << std::endl;
        std::cout << "--rmv-trace-file-name=<string>                The name of the RMV trace file." << std::endl;
        std::cout << "--rra-capture-start-frame=<number>            Which frame to start taking RRA capture from" << std::endl;
        std::cout << "--rra-capture-file-name=<string>              The name of the RRA capture file." << std::endl;
        std::cout << "--rra-enable-ray-history                      Enable ray history data collection for RRA." << std::endl;
        std::cout << "--rra-history-buffer-size=<number>            The ray history buffer size (0=minimum, 1=low, 2=default, 3=high, 4=maximum)." << std::endl;
        // Marker-based RRA capture flags (--rra-enable-marker-capture / --rra-marker-begin /
        // --rra-marker-end) are intentionally omitted from this help output. They are still
        // parsed below so existing scripts continue to work; they are not advertised to end
        // users. Note: the wrapper uses --rra-enable-marker-capture (see kOptionRraEnableMarkerCapture)
        // while the CLI uses --rra-marker-capture; both refer to the same underlying feature.
        std::cout << "--number-of-frames=<number>                   Number of frames to render before exiting (ignored when RGP or RMV is enabled)."
                  << std::endl;
        std::cout << "--log-to-file                                 Enables logging to a file." << std::endl;
        std::cout << "--log-file-name=<string>                      Sets the name of the log file." << std::endl;
        std::cout << "--dump-config                                 Writes the configuration to a config.txt file in the current working directory."
                  << std::endl;
    }

private:
    /// @brief Update the RGP state.
    void UpdateRGPState()
    {
        if (rgp_done_ || rgp_frames_captured_ >= config_.rgp_capture_count)
        {
            rgp_done_ = true;
            return;
        }

        if (dispatch_table_.get_feature_stage(context_, kRdpCaptureFeatureProfiling, 0) != kRdpCaptureFeatureStageReadyForCapture)
        {
            return;
        }

        if (config_.rgp_capture_start_frame > 0 && config_.rgp_capture_start_dispatch_index == -1 && config_.rgp_capture_dispatch_count == 0)
        {
            bool ready_to_capture_first_frame = rgp_frames_captured_ == 0 && frame_count_ == static_cast<uint64_t>(config_.rgp_capture_start_frame);
            bool ready_to_capture_next_frame  = rgp_frames_captured_ > 0 && rgp_timer_.TimeUp(config_.rgp_time_between_profiles);

            // Take an RGP profile every kRGPTimeBetweenProfiles frames
            // Notice here that TriggerCapture() may return false if the last capture hasn't completed.
            // In this case, the capture for this frame will be skipped.
            if (ready_to_capture_first_frame || ready_to_capture_next_frame)
            {
                rgp_timer_.StartTimer();
                if (rgp_table_.begin_trace(context_, RDP_CAPTURE_API_FIRST_CONNECTION_ID) != kRdpCaptureResultSuccess)
                {
                    rgp_done_ = true;
                }
            }
        }
    }

    /// @brief Update the RMV state.
    void UpdateRMVState()
    {
        if (rmv_done_)
        {
            return;
        }

        // Add an RMV snapshot if needed.
        if (rmv_timer_.TimeUp(config_.rmv_snapshot_time) && rmv_snapshot_taken_ == false)
        {
            rmv_table_.insert_snapshot(context_, RDP_CAPTURE_API_FIRST_CONNECTION_ID, config_.rmv_snapshot_name);
            rmv_snapshot_taken_ = true;
        }

        // If the RMV trace length has been reached and an RMV trace hasn't been triggered, trigger one.
        if (rmv_timer_.TimeUp(config_.rmv_trace_length) &&
            dispatch_table_.get_feature_stage(context_, kRdpCaptureFeatureMemoryTrace, 0) == kRdpCaptureFeatureStageCapturing)
        {
            if (rmv_table_.dump_trace(context_, RDP_CAPTURE_API_FIRST_CONNECTION_ID) != kRdpCaptureResultSuccess)
            {
                rmv_done_ = true;
            }
        }
    }

    void UpdateRRAState()
    {
        if (rra_done_)
        {
            return;
        }

        const bool ready_to_capture = config_.rra_capture_start_frame != 0 && frame_count_ == static_cast<uint64_t>(config_.rra_capture_start_frame);
        if (ready_to_capture)
        {
            if (rra_table_.begin_trace(context_, RDP_CAPTURE_API_FIRST_CONNECTION_ID) != kRdpCaptureResultSuccess)
            {
                rra_done_ = true;
            }
        }
    }

    /// @brief Get a number from a command line argument string.
    ///
    /// @param [in] string     The argument string.
    /// @param [out] out_value The number value.
    void GetNumberFromArgument(const char* string, int32_t* out_value)
    {
        const char* num_pointer = strstr(string, "=");
        if (num_pointer != nullptr)
        {
            // skip the '='
            num_pointer++;
            sscanf(num_pointer, "%d", out_value);
        }
    }

    /// @brief Get a number from a command line argument string.
    ///
    /// @param [in] string     The argument string.
    /// @param [out] out_value The number value.
    void GetNumberFromArgument(const char* string, uint32_t* out_value)
    {
        const char* num_pointer = strstr(string, "=");
        if (num_pointer != nullptr)
        {
            // skip the '='
            num_pointer++;
            sscanf(num_pointer, "%u", out_value);
        }
    }

    /// @brief Get a value string from a command line argument string.
    ///
    /// @param [in] string     The argument string containing the string value.
    /// @param [out] out_value The string value.
    void GetStringFromArgument(const char* string, char* out_value)
    {
        const char* string_pointer = strstr(string, "=");
        if (string_pointer != nullptr)
        {
            // skip the '='
            string_pointer++;
            strcpy(out_value, string_pointer);
        }
    }

    /// @brief Process a single command line argument.
    ///
    /// @param argument The command line argument string to process.
    void ProcessCommandLineArgument(const char* argument)
    {
        if (strcmp(argument, kOptionRmvEnable) == 0)
        {
            config_.rmv_enabled = true;
        }
        else if (strcmp(argument, kOptionRgpEnable) == 0)
        {
            config_.rgp_enabled = true;
        }
        else if (strcmp(argument, kOptionRraEnable) == 0)
        {
            config_.rra_enabled = true;
        }
        else if (strcmp(argument, kOptionRgpEnableInstructionTracing) == 0)
        {
            config_.rgp_instruction_tracing_enabled = true;
        }
        else if (strcmp(argument, kOptionRgpEnableCounters) == 0)
        {
            config_.rgp_counters_enabled = true;
        }
        else if (strstr(argument, kOptionRgpCaptureCount) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rgp_capture_count);
        }
        else if (strstr(argument, kOptionRgpCaptureStartFrame) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rgp_capture_start_frame);
        }
        else if (strstr(argument, kOptionRgpTimeBetweenProfiles) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rgp_time_between_profiles);
        }
        else if (strstr(argument, kOptionRgpCaptureStartDispatchIndex) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rgp_capture_start_dispatch_index);
        }
        else if (strstr(argument, kOptionRgpCaptureDispatchCount) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rgp_capture_dispatch_count);
        }
        else if (strstr(argument, kOptionRgpCaptureFileName) != nullptr)
        {
            GetStringFromArgument(argument, config_.rgp_capture_file_name);
            config_.rgp_capture_file_name_set = true;
        }
        else if (strstr(argument, kOptionRmvSnapshotTime) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rmv_snapshot_time);
        }
        else if (strstr(argument, kOptionRmvSnapshotName) != nullptr)
        {
            GetStringFromArgument(argument, config_.rmv_snapshot_name);
        }
        else if (strstr(argument, kOptionRmvTraceLength) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rmv_trace_length);
        }
        else if (strstr(argument, kOptionRmvTraceFileName) != nullptr)
        {
            GetStringFromArgument(argument, config_.rmv_trace_file_name);
            config_.rmv_trace_file_name_set = true;
        }
        else if (strstr(argument, kOptionRraCaptureStartFrame) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rra_capture_start_frame);
        }
        else if (strstr(argument, kOptionRraCaptureFileName) != nullptr)
        {
            GetStringFromArgument(argument, config_.rra_capture_file_name);
            config_.rra_capture_file_name_set = true;
        }
        else if (strstr(argument, kOptionRraEnableRayHistory) != nullptr)
        {
            config_.rra_enable_ray_history = true;
        }
        else if (strstr(argument, kOptionRraRayHistorySize) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.rra_ray_history_buffer_size);
        }
        else if (strcmp(argument, kOptionRraEnableMarkerCapture) == 0)
        {
            config_.rra_enable_marker_capture = true;
        }
        else if (strstr(argument, kOptionRraMarkerBegin) != nullptr)
        {
            GetStringFromArgument(argument, config_.rra_marker_begin);
        }
        else if (strstr(argument, kOptionRraMarkerEnd) != nullptr)
        {
            GetStringFromArgument(argument, config_.rra_marker_end);
        }
        else if (strstr(argument, kOptionNumberOfFrames) != nullptr)
        {
            GetNumberFromArgument(argument, &config_.number_of_frames);
        }
        else if (strcmp(argument, kOptionLogToFile) == 0)
        {
            config_.log_to_file = true;
        }
        else if (strstr(argument, kOptionLogFileName) != nullptr)
        {
            GetStringFromArgument(argument, config_.log_file_name);
        }
        else if (strcmp(argument, kOptionDumpConfig) == 0)
        {
            config_.dump_config = true;
        }
    }

    /// @brief Dump out the configuration settings to the current folder.
    ///
    /// Used for debugging at present to make sure options look correct.
    ///
    void DumpConfig()
    {
        FILE* fp = fopen("config.txt", "wt");
        if (fp)
        {
            fprintf(fp, "rgp_enabled = %s\n", config_.rgp_enabled ? "true" : "false");
            fprintf(fp, "rmv_enabled = %s\n", config_.rmv_enabled ? "true" : "false");
            fprintf(fp, "rra_enabled = %s\n", config_.rra_enabled ? "true" : "false");
            fprintf(fp, "rgp_capture_count = %d\n", config_.rgp_capture_count);
            fprintf(fp, "rgp_capture_start_frame = %d\n", config_.rgp_capture_start_frame);
            fprintf(fp, "rgp_time_between_profiles = %d\n", config_.rgp_time_between_profiles);
            fprintf(fp, "rgp_instruction_tracing_enabled = %s\n", config_.rgp_instruction_tracing_enabled ? "true" : "false");
            fprintf(fp, "rgp_counters_enabled = %s\n", config_.rgp_counters_enabled ? "true" : "false");
            fprintf(fp, "rgp_shader_mask = %d\n", config_.rgp_shader_mask);
            fprintf(fp, "rgp_spm_buffer_size = %d\n", config_.rgp_spm_buffer_size);
            fprintf(fp, "rgp_capture_file_name_set = %s\n", config_.rgp_capture_file_name_set ? "true" : "false");
            fprintf(fp, "rgp_capture_file_name = %s\n", config_.rgp_capture_file_name);
            fprintf(fp, "rgp_capture_start_dispatch_index = %d\n", config_.rgp_capture_start_dispatch_index);
            fprintf(fp, "rgp_capture_dispatch_count = %d\n", config_.rgp_capture_dispatch_count);
            fprintf(fp, "rmv_snapshot_time = %d\n", config_.rmv_snapshot_time);
            fprintf(fp, "rmv_snapshot_name = %s\n", config_.rmv_snapshot_name);
            fprintf(fp, "rmv_trace_length = %d\n", config_.rmv_trace_length);
            fprintf(fp, "rmv_trace_file_name_set = %s\n", config_.rmv_trace_file_name_set ? "true" : "false");
            fprintf(fp, "rmv_trace_file_name = %s\n", config_.rmv_trace_file_name);
            fprintf(fp, "rra_capture_start_frame = %d\n", config_.rra_capture_start_frame);
            fprintf(fp, "rra_capture_file_name_set = %s\n", config_.rra_capture_file_name_set ? "true" : "false");
            fprintf(fp, "rra_capture_file_name = %s\n", config_.rra_capture_file_name);
            fprintf(fp, "rra_enable_ray_history = %s\n", config_.rra_enable_ray_history ? "true" : "false");
            fprintf(fp, "rra_ray_history_buffer_size = %d\n", config_.rra_ray_history_buffer_size);
            fprintf(fp, "rra_enable_marker_capture = %s\n", config_.rra_enable_marker_capture ? "true" : "false");
            fprintf(fp, "rra_marker_begin = %s\n", config_.rra_marker_begin);
            fprintf(fp, "rra_marker_end = %s\n", config_.rra_marker_end);
            fprintf(fp, "number_of_frames = %d\n", config_.number_of_frames);
            fprintf(fp, "log_to_file = %d\n", config_.log_to_file);
            fprintf(fp, "log_file_name = %s\n", config_.log_file_name);
            fclose(fp);
        }
    }

    RdpCaptureContext                   context_;           ///< The DevDriverAPI context.
    RdpCaptureFnTable                   dispatch_table_{};  ///< the DevDriverAPI function dispatch table.
    RdpCaptureFeatureProfilingFnTable   rgp_table_;         ///< the RGP function dispatch table.
    RdpCaptureFeatureMemoryTraceFnTable rmv_table_;         ///< the RMV function dispatch table.
    RdpCaptureFeatureRaytracingFnTable  rra_table_;         ///< the RRA function dispatch table.

    CaptureConfig config_;               ///< Configuration structure holding all capture requirements.
    Timer         rgp_timer_;            ///< Timer used for RGP.
    Timer         rmv_timer_;            ///< Timer used for RMV.
    uint64_t      frame_count_;          ///< The frame counter. Incremented each time a frame is rendered.
    int32_t       rgp_frames_captured_;  ///< The number of RGP frames captured so far.
    bool          rgp_done_;             ///< Is all the RGP capturing done.
    bool          rmv_trace_triggered_;  ///< Has an RMV trace been triggered?
    bool          rmv_snapshot_taken_;   ///< Has an RMV snapshot been taken?
    bool          rmv_done_;             ///< Is the RMV capture done?
    bool          rra_done_;             ///< Is the RRA capture done?

    std::ofstream log_file_;  ///< The file stream to log to.
};

CaptureWrapper::CaptureWrapper()
{
    impl_ = new CaptureWrapperImpl();
}

CaptureWrapper::~CaptureWrapper()
{
    delete impl_;
}

void CaptureWrapper::ProcessCommandLine(int argc, char* argv[])
{
    impl_->ProcessCommandLine(argc, argv);
}

bool CaptureWrapper::Init()
{
    return impl_->Init();
}

void CaptureWrapper::DeviceCreated()
{
    impl_->DeviceCreated();
}

void CaptureWrapper::Close()
{
    impl_->Close();
}

bool CaptureWrapper::FrameUpdate()
{
    return impl_->FrameUpdate();
}

bool CaptureWrapper::PreDispatch()
{
    return impl_->PreDispatch();
}

bool CaptureWrapper::WaitUntilFinished()
{
    return impl_->WaitUntilFinished();
}

void CaptureWrapper::DisplayCommandLineHelp()
{
    impl_->DisplayCommandLineHelp();
}

CaptureWrapper& CaptureWrapper::Get()
{
    static CaptureWrapper dev_driver_wrapper;
    return dev_driver_wrapper;
}

void CaptureWrapper::LogLine(const char* log_line)
{
    impl_->Log(log_line);
}
