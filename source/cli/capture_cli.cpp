// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Capture CLI implementation for RadeonDeveloperPanelCLI.

#include "capture_cli.h"

#include <cxxopts.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <thread>
#include <type_traits>

#include <tl/expected.hpp>

#include <amdrdf.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// Built-in CLI blocklist. The header is generated at build time from
// source/frontend/blocklist.ini by source/cli/blocklist_gen.py and lives in
// CMAKE_CURRENT_BINARY_DIR. ApplyBlocklist() clears whatever the shared API
// library loaded from disk and applies these entries instead, so behavior is
// deterministic regardless of whether a stray blocklist.ini sits next to the binary.
// Must remain at file scope. Including it inside the anonymous namespace below
// would nest both the generated declarations and the header's transitive
// includes (notably <array>) into that namespace, which breaks std:: lookups.
#include "cli_blocklist_defaults.h"

namespace
{
    constexpr uint16_t         kDefaultRemotePort     = 27300;
    constexpr int              kSeparatorWidth        = 60;
    constexpr int              kHexRadix              = 16;
    constexpr std::string_view kHiddenRaytracingGroup = "Raytracing (hidden)";

    /// @brief Minimum legal value for the dispatch auto-capture start index.
    /// The trace clients (UberTrace render-op controller and legacy RGP client) treat
    /// 0 as "no trigger registered", which causes the capture to never fire and the
    /// CLI's wait loop to hang indefinitely. (The user-data mapper in
    /// source_userdata_mapper.cpp only supplies 1 as the default when the field is
    /// missing from JSON; it does not enforce the minimum on explicit zero values.)
    /// The GUI enforces the same minimum on its spinbox (kDispatchIndexMinimum in
    /// profiling_userdata_view.h).
    constexpr uint32_t kDispatchStartIndexMinimum = 1;

    /// @brief Minimum legal value for the frame auto-capture index.
    /// The UberTrace frame controller computes its preparation start index as
    /// (frame_capture_index - kNumPreparationFrames - 1), where kNumPreparationFrames is 4
    /// (see rgp_trace_client.h / rgp_ubertrace_client.cpp). Indices below 5 make this
    /// expression underflow the uint32_t, producing a huge start index so the capture never
    /// fires and the CLI wait loop hangs. The GUI enforces the same minimum on its spinbox
    /// (kFrameIndexMinimum in profiling_userdata_view.h).
    constexpr uint32_t kFrameCaptureIndexMinimum = 5;
    constexpr int      kPollingIntervalMs        = 100;
    constexpr uint64_t kHzPerMhz                 = 1000000;

    /// @brief Mutex that serializes all stdout/stderr writes in the CLI.
    /// Callbacks (StatusChanged, ProgressUpdated, LogCallback, TraceFinished, AppFilter)
    /// may fire concurrently from API threads; this mutex prevents interleaved output
    /// and keeps progress-bar clear/render sequences atomic.
    std::mutex g_output_mutex;

    /// @brief Safe character-level tolower that avoids UB with signed char.
    char ToLowerChar(unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    }

    /// @brief Returns the display name for an RGP capture mode value.
    const char* RgpCaptureModeName(uint32_t mode)
    {
        switch (mode)
        {
        case kRdpCaptureProfilingCaptureModeFrame:
            return "frame";
        case kRdpCaptureProfilingCaptureModeDraw:
            return "draw";
        case kRdpCaptureProfilingCaptureModeDispatch:
            return "dispatch";
        default:
            return "default";
        }
    }

    /// @brief Parse an integer from a string using std::from_chars (no exceptions).
    /// std::from_chars lacks overloads for int8_t/uint8_t on some implementations,
    /// so 8-bit types are parsed as a wider integer and then range-checked.
    template <typename T>
    tl::expected<T, std::string> ParseNumber(const std::string& str, int base = 10)
    {
        if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>)
        {
            using Wide  = std::conditional_t<std::is_signed_v<T>, int16_t, uint16_t>;
            auto result = ParseNumber<Wide>(str, base);
            if (!result)
            {
                return tl::make_unexpected(result.error());
            }
            if (result.value() < std::numeric_limits<T>::min() || result.value() > std::numeric_limits<T>::max())
            {
                return tl::make_unexpected("Value out of range for 8-bit type: " + str);
            }
            return static_cast<T>(result.value());
        }
        else
        {
            T value{};
            const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value, base);
            if (ec != std::errc{} || ptr != str.data() + str.size())
            {
                return tl::make_unexpected("Invalid numeric value: " + str);
            }
            return value;
        }
    }

    /// @brief Parse a float from a string using std::from_chars (no exceptions).

    CaptureCli* g_capture_cli_instance = nullptr;

    uint8_t AppFilterWrapper([[maybe_unused]] void* user_data, const RdpCaptureProcessInfo* process_info, const RdpCaptureGpuApi api)
    {
        if (g_capture_cli_instance != nullptr)
        {
            return g_capture_cli_instance->AppFilter(process_info, api) ? 1 : 0;
        }
        return 0;
    }

    void LogCallbackWrapper([[maybe_unused]] void* user_data, const RdpCaptureLogLevel level, const char* source, const uint32_t process_id, const char* msg)
    {
        if (g_capture_cli_instance != nullptr)
        {
            CaptureCli::LogCallback(level, source, process_id, msg);
        }
    }

    void BlockedProcessCallback([[maybe_unused]] void* user_data, const char* process_path, const uint32_t process_id)
    {
        const std::string                 app_name = std::filesystem::path(process_path).filename().string();
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Blocked process: " << app_name << " (PID: " << process_id << ")\n";
    }

    void TraceFinishedWrapper([[maybe_unused]] void*    user_data,
                              const RdpCaptureFeature   feature,
                              RdpCaptureApiConnectionId connection,
                              const RdpCaptureResult    result,
                              const uint64_t            size,
                              const uint8_t*            data)
    {
        if (g_capture_cli_instance != nullptr)
        {
            g_capture_cli_instance->TraceFinished(feature, connection, result, {data, size});
        }
    }

    void StatusCallbackWrapper([[maybe_unused]] void*        user_data,
                               const RdpCaptureFeature       feature,
                               const RdpCaptureDetailedStage new_stage,
                               const RdpCaptureDetailedStage old_stage)
    {
        if (g_capture_cli_instance != nullptr)
        {
            g_capture_cli_instance->StatusChanged(feature, new_stage, old_stage);
        }
    }

    void ProgressCallbackWrapper([[maybe_unused]] void* user_data, const RdpCaptureProgressInfo* info)
    {
        if (g_capture_cli_instance != nullptr)
        {
            g_capture_cli_instance->ProgressUpdated(info);
        }
    }

    std::vector<std::string> GetHelpGroupsForMode(const std::string& mode_str)
    {
        std::string mode = mode_str;
        std::ranges::transform(mode, mode.begin(), ToLowerChar);

        std::vector<std::string> groups = {"", "General", "Blocklist", "System Info"};

        if (mode == "profiling" || mode == "rgp")
        {
            groups.emplace_back("Profiling (RGP)");
        }
        else if (mode == "raytracing" || mode == "rra")
        {
            groups.emplace_back("Raytracing (RRA)");
        }
        else if (mode == "memory" || mode == "rmv")
        {
        }
        else if (mode == "crash" || mode == "rgd")
        {
            groups.emplace_back("Crash Analysis (RGD)");
        }
        else if (mode == "clocks")
        {
            groups.emplace_back("Clocks");
        }
        else
        {
            return {};
        }

        return groups;
    }

    bool ParseAutoCapture(const std::string& auto_capture_str, CaptureConfig& config)
    {
        // Split on ':' first so the frame and dispatch branches can both validate the
        // mode keyword exactly and enforce their documented segment counts. Without this,
        // a substring/prefix check would let typos like 'framex:5' or 'dispatchx:1:1'
        // sneak in and silently fall back to a default-frame capture.
        std::vector<std::string> parts;
        std::istringstream       auto_capture_stream(auto_capture_str);
        std::string              part;
        while (std::getline(auto_capture_stream, part, ':'))
        {
            parts.emplace_back(part);
        }
        // std::getline drops the empty segment after a final delimiter, so 'frame:5:'
        // would parse to {"frame", "5"} and 'dispatch:1:1:' to {"dispatch", "1", "1"} -
        // the trailing-colon typo would slip past the segment-count checks below as if
        // the colon weren't there. Push the missing empty segment back so those typos
        // fail fast like any other extra segment.
        if (!auto_capture_str.empty() && auto_capture_str.back() == ':')
        {
            parts.emplace_back();
        }
        const std::string& mode = parts.empty() ? auto_capture_str : parts[0];

        if (!parts.empty() && parts[0] == "frame")
        {
            config.auto_capture_mode = AutoCaptureMode::kFrameIndex;
            // Documented grammar is frame[:index] -> at most 2 colon-separated segments.
            // Reject extras so a typo like 'frame:5:extra' fails fast instead of silently
            // dropping the trailing tokens and running an unintended capture.
            if (parts.size() > 2)
            {
                std::cerr << "Error: --rgp-auto-capture frame expects at most 'frame[:index]' "
                          << "(got " << (parts.size() - 1) << " colon-separated argument(s) in '" << auto_capture_str << "').\n";
                return false;
            }
            if (parts.size() == 2)
            {
                // Distinguish empty (e.g. "frame:") from non-numeric (e.g. "frame:abc"):
                //   * empty   -> use default frame 0.
                //   * number  -> use parsed value.
                //   * garbage -> hard error so a typo never silently falls back to frame 0.
                if (parts[1].empty())
                {
                    config.frame_capture_index = 0;
                }
                else
                {
                    const auto parsed = ParseNumber<uint32_t>(parts[1]);
                    if (!parsed.has_value())
                    {
                        std::cerr << "Error: --rgp-auto-capture frame index '" << parts[1] << "' is not a valid unsigned integer.\n";
                        return false;
                    }
                    config.frame_capture_index = parsed.value();
                }
            }
            else
            {
                // Bare "frame" with no index -> use the minimum valid frame index.
                config.frame_capture_index = kFrameCaptureIndexMinimum;
            }
            if (std::cmp_less(config.frame_capture_index, kFrameCaptureIndexMinimum))
            {
                std::cerr << "Error: --rgp-auto-capture frame index must be >= " << kFrameCaptureIndexMinimum << " (got " << config.frame_capture_index
                          << "). The trace controller reserves " << "the first few frames for capture preparation,"
                          << " so lower indices cause the capture to never fire.\n";
                return false;
            }
        }
        else if (!parts.empty() && parts[0] == "dispatch")
        {
            config.auto_capture_mode = AutoCaptureMode::kDispatchIndex;
            // The documented grammar is dispatch[:start[:count]] -> at most 3 colon-separated
            // segments. Reject extras so that a typo like 'dispatch:1:1:5' or
            // 'dispatch:1:1:garbage' fails fast instead of silently dropping the trailing
            // tokens and running an unintended capture.
            if (parts.size() > 3)
            {
                std::cerr << "Error: --rgp-auto-capture dispatch expects at most 'dispatch[:start[:count]]' "
                          << "(got " << (parts.size() - 1) << " colon-separated argument(s) in '" << auto_capture_str << "').\n";
                return false;
            }
            // For each numeric segment, distinguish three cases:
            //   * segment omitted/empty (e.g. "dispatch", "dispatch::5") -> use default.
            //   * segment parses as a number                              -> use parsed value.
            //   * segment contains non-numeric text (e.g. "dispatch:abc:1") -> hard error,
            //     so that a typo never silently falls back to the default and produces an
            //     unintended capture.
            if (parts.size() >= 2)
            {
                if (parts[1].empty())
                {
                    config.dispatch_start_index = kDispatchStartIndexMinimum;
                }
                else
                {
                    const auto parsed = ParseNumber<uint32_t>(parts[1]);
                    if (!parsed.has_value())
                    {
                        std::cerr << "Error: --rgp-auto-capture dispatch start index '" << parts[1] << "' is not a valid unsigned integer.\n";
                        return false;
                    }
                    config.dispatch_start_index = parsed.value();
                }
            }
            if (parts.size() >= 3)
            {
                if (parts[2].empty())
                {
                    config.dispatch_count = 1;
                }
                else
                {
                    const auto parsed = ParseNumber<uint32_t>(parts[2]);
                    if (!parsed.has_value())
                    {
                        std::cerr << "Error: --rgp-auto-capture dispatch count '" << parts[2] << "' is not a valid unsigned integer.\n";
                        return false;
                    }
                    config.dispatch_count = parsed.value();
                    if (config.dispatch_count == 0)
                    {
                        config.dispatch_count = 1;
                    }
                }
            }
            if (config.dispatch_start_index < kDispatchStartIndexMinimum)
            {
                std::cerr << "Error: --rgp-auto-capture dispatch start index must be >= " << kDispatchStartIndexMinimum << " (got "
                          << config.dispatch_start_index << "). The driver treats 0 as 'no trigger registered',"
                          << " which would cause the capture to never fire.\n";
                return false;
            }
        }
        else
        {
            // An unknown mode (e.g. 'fram', 'foo') must fail rather than silently falling
            // through to a default-frame capture, otherwise a typo runs an unintended trace.
            std::cerr << "Error: --rgp-auto-capture mode '" << mode << "' is not recognized. Expected 'frame[:index]' or 'dispatch[:start[:count]]'.\n";
            return false;
        }
        return true;
    }

    void ParseSqttBufferSize(const std::string& sqtt_str, CaptureConfig& config)
    {
        config.sqtt_buffer_size = kRdpCaptureSqttBufferSizeDefault;
        if (sqtt_str == "minimum")
        {
            config.sqtt_buffer_size = kRdpCaptureSqttBufferSizeMinimum;
        }
        else if (sqtt_str == "low")
        {
            config.sqtt_buffer_size = kRdpCaptureSqttBufferSizeLow;
        }
        else if (sqtt_str == "high")
        {
            config.sqtt_buffer_size = kRdpCaptureSqttBufferSizeHigh;
        }
        else if (sqtt_str == "maximum")
        {
            config.sqtt_buffer_size = kRdpCaptureSqttBufferSizeMaximum;
        }
    }

    void ParseRraBufferSize(const std::string& rra_buf_str, CaptureConfig& config)
    {
        config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeDefault;
        if (rra_buf_str == "disabled")
        {
            config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeRayHistoryDisabled;
        }
        else if (rra_buf_str == "minimum")
        {
            config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeMinimum;
        }
        else if (rra_buf_str == "low")
        {
            config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeLow;
        }
        else if (rra_buf_str == "high")
        {
            config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeHigh;
        }
        else if (rra_buf_str == "maximum")
        {
            config.rra_ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeMaximum;
        }
    }

    bool ParseRgpCaptureMode(const std::string& mode_str, CaptureConfig& config)
    {
        config.rgp_capture_mode = kRdpCaptureProfilingCaptureModeDefault;
        if (mode_str == "default")
        {
            config.rgp_capture_mode = kRdpCaptureProfilingCaptureModeDefault;
        }
        else if (mode_str == "frame")
        {
            config.rgp_capture_mode = kRdpCaptureProfilingCaptureModeFrame;
        }
        else if (mode_str == "draw")
        {
            config.rgp_capture_mode = kRdpCaptureProfilingCaptureModeDraw;
        }
        else if (mode_str == "dispatch")
        {
            config.rgp_capture_mode = kRdpCaptureProfilingCaptureModeDispatch;
        }
        else
        {
            std::cerr << "Invalid RGP capture mode: '" << mode_str << "'. Expected: default, frame, draw, dispatch\n";
            return false;
        }
        return true;
    }

    /// @brief Single source of truth for one CLI option (cxxopts registration + mode-flag validation).
    struct OptionSpec
    {
        const char*                     names;        ///< Cxxopts name field, e.g. "rgd-enhanced" or "a,rgp-auto-capture".
        const char*                     description;  ///< Help text.
        std::shared_ptr<cxxopts::Value> value;        ///< Cxxopts value spec; nullptr means a no-value boolean flag.
    };

    /// @brief A help-grouped collection of options, optionally tied to a single capture mode.
    struct OptionGroup
    {
        std::optional<CaptureMode> mode;        ///< Capture mode that owns this group; nullopt = always available.
        const char*                group_name;  ///< Cxxopts help group label.
        std::vector<OptionSpec>    options;
    };

    /// @brief Extract the long flag name from a cxxopts name field of the form "short,long" or "long".
    std::string LongFlagName(const char* names)
    {
        const std::string_view view{names};
        const auto             comma = view.find(',');
        return std::string{comma == std::string_view::npos ? view : view.substr(comma + 1)};
    }

    /// @brief Map a capture mode to the value users pass to --mode / --help.
    const char* ModeHelpName(const CaptureMode mode)
    {
        switch (mode)
        {
        case CaptureMode::kProfiling:
            return "profiling";
        case CaptureMode::kRaytracing:
            return "raytracing";
        case CaptureMode::kMemoryTrace:
            return "memory";
        case CaptureMode::kCrashAnalysis:
            return "crash";
        case CaptureMode::kClocks:
            return "clocks";
        }
        return "profiling";
    }

    /// @brief Build the master list of CLI options.
    ///
    /// This is the single place where flag names, help text, value types, defaults, and the
    /// owning capture mode are defined. ``RegisterCliOptions`` feeds it to cxxopts for both
    /// argument parsing and ``--help`` output, and ``ValidateModeFlags`` walks the same list to
    /// reject flags that don't belong to the active mode. Adding a new option in one place
    /// automatically wires it through both paths.
    const std::vector<OptionGroup>& GetCliOptionGroups()
    {
        static const std::vector<OptionGroup> kGroups = {
            {std::nullopt,
             "General",
             {
                 {"o,output", "Output file path (default: <mode>_<timestamp>.<ext>)", cxxopts::value<std::string>()},
                 {"m,mode", "Capture mode: profiling, raytracing, memory, crash, clocks", cxxopts::value<std::string>()->default_value("profiling")},
                 {"p,process", "Filter: only connect to processes containing this string", cxxopts::value<std::string>()},
                 {"verbose", "Enable verbose logging", nullptr},
                 {"remote-host", "Remote hostname or IP address to connect to", cxxopts::value<std::string>()},
                 {"remote-port", "Remote port", cxxopts::value<uint16_t>()->default_value(std::to_string(kDefaultRemotePort))},
                 {"h,help", "Print usage (use --help=<mode> for mode-specific options)", cxxopts::value<std::string>()->implicit_value("")},
                 {"v,version", "Print version", nullptr},
             }},

            {CaptureMode::kProfiling,
             "Profiling (RGP)",
             {
                 {"a,rgp-auto-capture",
                  "Auto-capture mode: 'frame[:N]' to capture at frame N (default 0), "
                  "'dispatch[:start[:count]]' to capture dispatches (start must be >= 1, default 1; count default 1). "
                  "Examples: --rgp-auto-capture=frame, --rgp-auto-capture=frame:5, --rgp-auto-capture=dispatch:1:10",
                  cxxopts::value<std::string>()},
                 {"rgp-auto-capture-delay", "Delay in milliseconds before dispatch auto-capture starts", cxxopts::value<uint32_t>()->default_value("0")},
                 {"rgp-capture-mode",
                  "Capture mode: default, frame, draw, dispatch. 'default' lets the driver choose based on the API",
                  cxxopts::value<std::string>()->default_value("default")},
                 {"rgp-render-op-count",
                  "Number of render operations to capture in draw or dispatch mode (ignored in frame mode)",
                  cxxopts::value<uint32_t>()->default_value("1")},
                 {"rgp-instruction-tracing", "Enable instruction-level tracing for detailed shader analysis", nullptr},
                 {"rgp-counter-collection", "Enable hardware counter collection", nullptr},
                 {"rgp-shader-instrumentation", "Enable shader instrumentation", nullptr},
                 {"rgp-sqtt-buffer-size", "SQTT buffer size: minimum, low, default, high, maximum", cxxopts::value<std::string>()->default_value("default")},
             }},

            {CaptureMode::kRaytracing,
             "Raytracing (RRA)",
             {
                 {"rra-ray-history-buffer-size",
                  "Ray history buffer size: disabled, minimum, low, default, high, maximum",
                  cxxopts::value<std::string>()->default_value("default")},
                 {"rra-collect-ray-dispatch-data",
                  "Collect ray dispatch data (ray history). Pass --rra-collect-ray-dispatch-data=false to disable; equivalent to "
                  "--rra-ray-history-buffer-size=disabled.",
                  cxxopts::value<bool>()->default_value("true")->no_implicit_value()},
                 {"rra-delay-ms",
                  "Delay in milliseconds before triggering each raytracing capture (0 = no delay)",
                  cxxopts::value<uint32_t>()->default_value("0")},
             }},

            // Hidden raytracing options: registered with cxxopts so they remain parseable, but
            // intentionally placed in a group that is excluded from --help output (see
            // ordered_help_sections / kHiddenGroupNames). Used to discreetly hide marker-based
            // capture from the public-facing help while keeping the existing flags functional.
            {CaptureMode::kRaytracing,
             kHiddenRaytracingGroup.data(),
             {
                 {"rra-marker-capture", "Enable marker-based capture instead of frame-based", nullptr},
                 {"rra-marker-begin", "Marker string that starts the capture (requires --rra-marker-capture)", cxxopts::value<std::string>()},
                 {"rra-marker-end", "Marker string that ends the capture (requires --rra-marker-capture)", cxxopts::value<std::string>()},
             }},

            {CaptureMode::kCrashAnalysis,
             "Crash Analysis (RGD)",
             {
                 {"rgd-enhanced", "Enable enhanced crash analysis (may affect application performance)", nullptr},
                 {"rgd-text-summary",
                  "Request a text crash summary alongside the .rgd (consumed by downstream tooling such as the RDP UI or rgd.exe)",
                  nullptr},
                 {"rgd-json-summary",
                  "Request a JSON crash summary alongside the .rgd (consumed by downstream tooling such as the RDP UI or rgd.exe)",
                  nullptr},
                 {"rgd-marker-source", "Display execution-marker source information in the crash summary", nullptr},
                 {"rgd-expand-markers", "Expand all execution-marker nodes in the crash summary", nullptr},
                 {"rgd-pdb-search-path", "DXC shader PDB search path; can be specified multiple times", cxxopts::value<std::vector<std::string>>()},
                 {"rgd-pdb-include-subfolders", "Recurse into subfolders when resolving DXC shader PDBs", nullptr},
                 {"rgd-cli-path",
                  "Path to the rgd executable used to generate text / JSON summaries (file or directory). Defaults to the current working directory.",
                  cxxopts::value<std::string>()},
                 {"rgd-collect-sgprs", "Collect wave SGPRs during enhanced crash analysis", nullptr},
                 {"rgd-collect-vgprs", "Collect wave VGPRs during enhanced crash analysis", nullptr},
             }},

            {CaptureMode::kClocks,
             "Clocks",
             {
                 {"clock-mode", "Clock mode to set: normal, stable", cxxopts::value<std::string>()},
                 {"gpu-index", "GPU index to target", cxxopts::value<uint64_t>()->default_value("0")},
             }},

            {std::nullopt,
             "Blocklist",
             {
                 {"block",
                  "Block a process name pattern from connecting (supported syntax: *, ?, [...], and backslash escaping); can be specified multiple times",
                  cxxopts::value<std::vector<std::string>>()},
                 {"block-file",
                  "Load additional blocklist entries from a text file (one pattern per line; lines starting with # are comments)",
                  cxxopts::value<std::string>()},
                 {"list-blocklist", "List all active blocklist entries and exit", nullptr},
             }},

            {std::nullopt,
             "System Info",
             {
                 {"system-info", "Print system info (OS, driver, CPUs, GPUs) and exit", nullptr},
             }},
        };
        return kGroups;
    }

    /// @brief Register every option from ``GetCliOptionGroups()`` with cxxopts.
    void RegisterCliOptions(cxxopts::Options& options)
    {
        for (const auto& group : GetCliOptionGroups())
        {
            auto adder = options.add_options(group.group_name);
            for (const auto& opt : group.options)
            {
                if (opt.value)
                {
                    adder(opt.names, opt.description, opt.value);
                }
                else
                {
                    adder(opt.names, opt.description);
                }
            }
        }
    }

    bool ParseCaptureMode(const std::string& mode_str, CaptureConfig& config)
    {
        if (mode_str == "profiling" || mode_str == "rgp")
        {
            config.mode = CaptureMode::kProfiling;
        }
        else if (mode_str == "raytracing" || mode_str == "rra")
        {
            config.mode = CaptureMode::kRaytracing;
        }
        else if (mode_str == "memory" || mode_str == "rmv")
        {
            config.mode = CaptureMode::kMemoryTrace;
        }
        else if (mode_str == "crash" || mode_str == "rgd")
        {
            config.mode = CaptureMode::kCrashAnalysis;
        }
        else if (mode_str == "clocks")
        {
            config.mode = CaptureMode::kClocks;
        }
        else
        {
            std::cerr << "Invalid mode: " << mode_str << '\n';
            return false;
        }
        return true;
    }

    /// @brief Reject flags that belong to a different capture mode.
    ///
    /// cxxopts itself groups flags only for help output; it happily accepts any registered flag
    /// regardless of --mode. Without this check, e.g. ``RadeonDeveloperPanelCLI --rgd-enhanced``
    /// silently runs profiling mode because --mode defaults to "profiling" and --rgd-enhanced is
    /// parsed but never applied.
    ///
    /// The mode → exclusive-flag mapping is sourced from ``GetCliOptionGroups()`` (groups whose
    /// ``mode`` has a value), so adding a new flag to that table automatically gates it here too.
    bool ValidateModeFlags(const cxxopts::ParseResult& result, const CaptureMode active_mode, const bool mode_explicit)
    {
        std::vector<std::pair<std::string, const char*>> offending;  // (flag, owning-mode help name)
        for (const auto& group : GetCliOptionGroups())
        {
            if (!group.mode.has_value() || *group.mode == active_mode)
            {
                continue;
            }
            for (const auto& opt : group.options)
            {
                std::string flag = LongFlagName(opt.names);
                if (result.count(flag) > 0)
                {
                    offending.emplace_back(std::move(flag), ModeHelpName(*group.mode));
                }
            }
        }

        if (offending.empty())
        {
            return true;
        }

        const char* active_name = ModeHelpName(active_mode);
        std::cerr << "Invalid flag combination: ";
        if (mode_explicit)
        {
            std::cerr << "--mode=" << active_name << " was specified, but the following flag(s) belong to a different mode:\n";
        }
        else
        {
            std::cerr << "no --mode specified (defaults to '" << active_name << "'), but the following flag(s) belong to a different mode:\n";
        }
        for (const auto& [flag, owner] : offending)
        {
            std::cerr << "  --" << flag << "  (--mode=" << owner << ")\n";
        }
        std::cerr << "Pass --mode=<mode> to select the matching capture mode, or remove the offending flag(s).\n";
        std::cerr << "Run --help=<mode> for the per-mode flag list.\n";
        return false;
    }

    bool ParseRemoteConnection(const cxxopts::ParseResult& result, CaptureConfig& config)
    {
        config.remote_host.clear();
        config.remote_port = 0;

        if (result.count("remote-host") != 0)
        {
            config.remote_host = result["remote-host"].as<std::string>();
            if (config.remote_host.empty())
            {
                std::cerr << "Remote host must not be empty\n";
                return false;
            }
            config.remote_port = result["remote-port"].as<uint16_t>();
            return true;
        }

        if (result.count("remote-port") != 0)
        {
            std::cerr << "--remote-port requires --remote-host\n";
            return false;
        }
        return true;
    }

    // Returns "" on success or an error message
    std::string TriggerCaptureForMode(const CaptureMode mode, const RdpCaptureFnTable& fn_table, RdpCaptureContext context)
    {
        switch (mode)
        {
        case CaptureMode::kProfiling:
        {
            if (const RdpCaptureResult res = fn_table.profiling.begin_trace(context, RDP_CAPTURE_API_FIRST_CONNECTION_ID); res != kRdpCaptureResultSuccess)
            {
                return "Failed to trigger capture (result: " + std::to_string(res) + ")";
            }
            return "";
        }
        case CaptureMode::kRaytracing:
        {
            if (const RdpCaptureResult res = fn_table.raytracing.begin_trace(context, RDP_CAPTURE_API_FIRST_CONNECTION_ID); res != kRdpCaptureResultSuccess)
            {
                return "Failed to trigger capture (result: " + std::to_string(res) + ")";
            }
            return "";
        }
        case CaptureMode::kMemoryTrace:
        {
            if (const RdpCaptureResult res = fn_table.memory_trace.dump_trace(context, RDP_CAPTURE_API_FIRST_CONNECTION_ID); res != kRdpCaptureResultSuccess)
            {
                return "Failed to trigger capture (result: " + std::to_string(res) + ")";
            }
            return "";
        }
        case CaptureMode::kCrashAnalysis:
            return "Crash analysis does not support manual capture triggers";
        case CaptureMode::kClocks:
            return "Clocks mode does not support capture triggers";
        }
        return "";
    }
}  // namespace

CaptureCli::CaptureCli(CaptureConfig config)
    : config_(std::move(config))
    , fn_table_{}
    , context_(nullptr)
    , capture_complete_(false)
    , interrupted_(false)
    , capture_success_(false)
    , progress_active_(false)
{
    g_capture_cli_instance = this;
}

CaptureCli::~CaptureCli()
{
    Cleanup();
    g_capture_cli_instance = nullptr;
}

void CaptureCli::Interrupt()
{
    // Just set the flag - cleanup will happen in the main thread
    interrupted_.store(true);
    capture_complete_.store(true);
}

int CaptureCli::Run()
{
    // One-shot info-printing flags don't run a capture mode; suppress the
    // "<Mode> Mode" banner so --system-info / --list-blocklist / etc. emit
    // only the requested data and don't show a spurious "Profiling (RGP)
    // Mode" header just because profiling is the default.
    const bool info_only_request = (config_.list_blocklist || config_.system_info);
    if (!info_only_request)
    {
        std::cout << "RadeonDeveloperPanelCLI - " << GetModeName() << " Mode\n";
        std::cout << "========================================\n";
    }

    if (!Initialize())
    {
        std::cerr << "Failed to initialize capture API\n";
        return 1;
    }

    ApplyBlocklist();
    WarnIfEtwNotConfigured();

    // One-shot info-printing flags: --list-blocklist, --system-info.
    // If any combination is requested, run all of them in a stable
    // order and exit (without running clocks/profiling/etc).  This way `--system-info --list-blocklist`
    // prints both before exiting, and the flags work uniformly regardless of --mode.
    bool any_info_request = info_only_request;
    if (any_info_request)
    {
        int exit_code = 0;
        if (config_.list_blocklist)
        {
            ListBlocklist();
        }
        if (config_.system_info)
        {
            PrintSystemInfo();
        }
        Cleanup();
        return exit_code;
    }

    if (config_.mode == CaptureMode::kClocks)
    {
        const int result = RunClocksMode();
        Cleanup();
        return result;
    }

    if (!EnableFeature())
    {
        std::cerr << "Failed to enable " << GetModeName() << " feature\n";
        Cleanup();
        return 1;
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Waiting for GPU application to connect...\n";
        if (!config_.process_filter.empty())
        {
            std::cout << "Process filter: " << config_.process_filter << '\n';
        }
        std::cout << "Press Ctrl+C to exit.\n\n";
    }

    if (config_.mode == CaptureMode::kCrashAnalysis)
    {
        return WaitForCrashAndSave();
    }

    if (config_.auto_capture_mode != AutoCaptureMode::kNone)
    {
        return WaitForAutoCaptureAndSave();
    }

    return RunManualCaptureLoop();
}

int CaptureCli::WaitForCrashAndSave()
{
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Monitoring for GPU crashes...\n";
        if (config_.enable_enhanced_crash_analysis)
        {
            std::cout << "Enhanced crash analysis is enabled (may affect application performance)\n";
        }
    }

    while (!capture_complete_.load() && !interrupted_.load())
    {
        const RdpCaptureFeatureStage stage = fn_table_.get_feature_stage(context_, GetFeature(), 0);
        // Not fatal: an app using an unsupported API connected. Keep waiting for
        // a supported app instead of exiting (StatusChanged explains it to the user).
        if (stage == kRdpCaptureFeatureStageApiUnsupported)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
            continue;
        }
        if (stage < 0)
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                if (progress_active_)
                {
                    ClearProgressLine();
                    progress_active_ = false;
                }
                std::cerr << "Feature is disabled or encountered an error (stage: " << stage << ")\n";
            }
            Cleanup();
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
    }

    if (interrupted_.load())
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            if (progress_active_)
            {
                ClearProgressLine();
                progress_active_ = false;
            }
            std::cout << "\nInterrupted by user, cleaning up...\n";
        }
        Cleanup();
        return 1;
    }

    if (capture_success_ && !trace_data_.empty())
    {
        const std::string output_path = GenerateOutputPath();

        if (std::ofstream file(output_path, std::ios::binary); file.is_open())
        {
            file.write(reinterpret_cast<const char*>(trace_data_.data()), static_cast<std::streamsize>(trace_data_.size()));
            file.close();
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Crash dump saved to: " << output_path << '\n';
            std::cout << "Size: " << trace_data_.size() << " bytes\n";
        }
        else
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                std::cerr << "Failed to write crash dump file: " << output_path << '\n';
            }
            Cleanup();
            return 1;
        }

        if (config_.rgd_generate_text_summary || config_.rgd_generate_json_summary)
        {
            GenerateCrashSummaries(output_path);
        }
    }
    else if (!capture_success_)
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Crash dump capture failed\n";
        }
        Cleanup();
        return 1;
    }

    Cleanup();
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Done!\n";
    }
    return 0;
}

namespace
{
    /// @brief Resolve the rgd executable based on a user-provided path (file or directory) or fall back to the current working directory.
    std::filesystem::path ResolveRgdExecutable(const std::string& user_path)
    {
        namespace fs = std::filesystem;
#ifdef _WIN32
        const std::string default_name = "rgd.exe";
#else
        const std::string default_name = "rgd";
#endif
        std::error_code ec;
        if (!user_path.empty())
        {
            fs::path candidate(user_path);
            if (fs::is_directory(candidate, ec))
            {
                candidate /= default_name;
            }
            return candidate;
        }
        return fs::current_path(ec) / default_name;
    }

    /// @brief Escape a single argument according to the rules consumed by
    /// CommandLineToArgvW / the Microsoft C runtime parser. In particular, runs of
    /// backslashes that precede a literal double quote (or the closing quote of a
    /// quoted argument) must be doubled.
#ifdef _WIN32
    std::wstring QuoteArgWindows(const std::wstring& arg)
    {
        const bool needs_quoting = arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
        if (!needs_quoting)
        {
            return arg;
        }

        std::wstring out;
        out.reserve(arg.size() + 2);
        out.push_back(L'"');
        for (auto it = arg.begin();; ++it)
        {
            size_t backslashes = 0;
            while (it != arg.end() && *it == L'\\')
            {
                ++backslashes;
                ++it;
            }
            if (it == arg.end())
            {
                out.append(backslashes * 2, L'\\');
                break;
            }
            if (*it == L'"')
            {
                out.append(backslashes * 2 + 1, L'\\');
                out.push_back(L'"');
            }
            else
            {
                out.append(backslashes, L'\\');
                out.push_back(*it);
            }
        }
        out.push_back(L'"');
        return out;
    }
#endif

    /// @brief Result of attempting to spawn and wait on a child process.
    struct ProcessResult
    {
        bool        spawned   = false;  ///< True if the child was launched at all.
        int         exit_code = -1;     ///< Child exit code if spawned and exited normally.
        unsigned    os_error  = 0;      ///< errno (POSIX) / GetLastError() (Windows) on spawn failure.
        std::string os_message;         ///< Human-readable description of os_error.
    };

#ifdef _WIN32
    /// @brief Spawn a child process with a specific working directory and wait for it to exit.
    /// argv[0] is used as the application path; all paths are passed as native wide strings so
    /// non-ASCII characters survive without depending on the active code page.
    ProcessResult RunProcessWithCwd(const std::vector<std::filesystem::path>& argv, const std::filesystem::path& working_dir)
    {
        ProcessResult result;
        if (argv.empty())
        {
            result.os_message = "no executable provided";
            return result;
        }

        std::wstring command_line;
        for (size_t i = 0; i < argv.size(); ++i)
        {
            if (i > 0)
            {
                command_line.push_back(L' ');
            }
            command_line += QuoteArgWindows(argv[i].native());
        }

        const std::wstring application = argv.front().native();
        const std::wstring cwd_w       = working_dir.native();

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};

        const BOOL ok = ::CreateProcessW(
            application.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, cwd_w.empty() ? nullptr : cwd_w.c_str(), &startup, &process);
        if (!ok)
        {
            const DWORD err        = ::GetLastError();
            result.os_error        = err;
            char*       msg_buffer = nullptr;
            const DWORD len        = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                               nullptr,
                                               err,
                                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                               reinterpret_cast<LPSTR>(&msg_buffer),
                                               0,
                                               nullptr);
            if (len > 0 && msg_buffer != nullptr)
            {
                result.os_message.assign(msg_buffer, len);
                while (!result.os_message.empty() && (result.os_message.back() == '\r' || result.os_message.back() == '\n'))
                {
                    result.os_message.pop_back();
                }
            }
            if (msg_buffer != nullptr)
            {
                ::LocalFree(msg_buffer);
            }
            return result;
        }

        result.spawned = true;
        ::WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exit_code = 0;
        ::GetExitCodeProcess(process.hProcess, &exit_code);
        ::CloseHandle(process.hProcess);
        ::CloseHandle(process.hThread);
        result.exit_code = static_cast<int>(exit_code);
        return result;
    }
#else
    /// @brief Spawn a child process with a specific working directory and wait for it to exit.
    /// Uses fork + chdir + execvp so no shell parses the command (avoids quoting/injection issues)
    /// and surfaces the child's actual exit code via WEXITSTATUS.
    ProcessResult RunProcessWithCwd(const std::vector<std::filesystem::path>& argv, const std::filesystem::path& working_dir)
    {
        ProcessResult result;
        if (argv.empty())
        {
            result.os_message = "no executable provided";
            return result;
        }

        std::vector<std::string> arg_storage;
        arg_storage.reserve(argv.size());
        for (const auto& p : argv)
        {
            arg_storage.push_back(p.string());
        }
        std::vector<char*> argv_c;
        argv_c.reserve(arg_storage.size() + 1);
        for (auto& s : arg_storage)
        {
            argv_c.push_back(s.data());
        }
        argv_c.push_back(nullptr);

        const pid_t pid = fork();
        if (pid < 0)
        {
            result.os_error   = static_cast<unsigned>(errno);
            result.os_message = std::strerror(errno);
            return result;
        }
        if (pid == 0)
        {
            if (!working_dir.empty())
            {
                if (chdir(working_dir.c_str()) != 0)
                {
                    _exit(127);
                }
            }
            execvp(argv_c[0], argv_c.data());
            _exit(127);
        }

        result.spawned = true;
        int status     = 0;
        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                result.spawned    = false;
                result.os_error   = static_cast<unsigned>(errno);
                result.os_message = std::strerror(errno);
                return result;
            }
        }
        if (WIFEXITED(status))
        {
            result.exit_code = WEXITSTATUS(status);
        }
        return result;
    }
#endif
}  // namespace

void CaptureCli::GenerateCrashSummaries(const std::string& dump_path) const
{
    namespace fs = std::filesystem;

    fs::path        rgd_exe = ResolveRgdExecutable(config_.rgd_cli_path);
    std::error_code ec;
    if (!fs::exists(rgd_exe, ec) || !fs::is_regular_file(rgd_exe, ec))
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "Cannot generate crash summary: rgd executable not found at " << rgd_exe.string() << '\n';
        std::cerr << "  Use --rgd-cli-path to point to its location.\n";
        return;
    }

    // rgd resolves helper tools (e.g. dxc.exe) relative to its own working directory, so we
    // launch it with cwd = its containing directory. Normalise the path to absolute first so
    // any relative components in the user-provided --rgd-cli-path don't become unresolvable
    // after the working-directory change. All other paths handed to rgd are also absolute.
    rgd_exe                      = fs::absolute(rgd_exe, ec);
    const fs::path rgd_dir       = rgd_exe.parent_path();
    const fs::path absolute_dump = fs::absolute(fs::path(dump_path), ec);

    auto invoke = [&](const std::string& output_arg, const fs::path& output_path, const char* label) {
        std::vector<fs::path> argv;
        argv.reserve(16);
        argv.push_back(rgd_exe);
        argv.emplace_back("--parse");
        argv.push_back(absolute_dump);
        argv.emplace_back(output_arg);
        argv.push_back(output_path);
        if (config_.rgd_show_marker_source)
        {
            argv.emplace_back("--marker-src");
        }
        if (config_.rgd_expand_markers)
        {
            argv.emplace_back("--expand-markers");
        }
        for (const std::string& path : config_.rgd_pdb_search_paths)
        {
            if (path.empty())
            {
                continue;
            }
            argv.emplace_back("--pdb-path");
            argv.push_back(fs::absolute(fs::path(path), ec));
        }
        if (config_.rgd_pdb_include_subfolders)
        {
            argv.emplace_back("--pdb-subdir");
        }

        const ProcessResult               rc = RunProcessWithCwd(argv, rgd_dir);
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        if (!rc.spawned)
        {
            std::cerr << label << " summary generation failed: could not launch rgd at " << rgd_exe.string() << " (cwd=" << rgd_dir.string()
                      << "): " << (rc.os_message.empty() ? "spawn failed" : rc.os_message) << " (os error " << rc.os_error << ")\n";
            return;
        }
        if (rc.exit_code == 0 && fs::exists(output_path, ec) && fs::file_size(output_path, ec) > 0)
        {
            std::cout << label << " summary saved to: " << output_path.string() << '\n';
        }
        else
        {
            std::cerr << label << " summary generation failed (rgd exit code " << rc.exit_code << ")\n";
        }
    };

    if (config_.rgd_generate_text_summary)
    {
        fs::path text_path = absolute_dump;
        text_path.replace_extension(".txt");
        invoke("--output", text_path, "Text");
    }
    if (config_.rgd_generate_json_summary)
    {
        fs::path json_path = absolute_dump;
        json_path.replace_extension(".json");
        invoke("--json", json_path, "JSON");
    }
}

int CaptureCli::WaitForAutoCaptureAndSave()
{
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Auto-capture enabled, waiting for capture to complete...\n";
    }

    while (!capture_complete_.load() && !interrupted_.load())
    {
        const RdpCaptureFeatureStage stage = fn_table_.get_feature_stage(context_, GetFeature(), 0);
        // Not fatal: an app using an unsupported API connected. Keep waiting for
        // a supported app instead of exiting (StatusChanged explains it to the user).
        if (stage == kRdpCaptureFeatureStageApiUnsupported)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
            continue;
        }
        if (stage < 0)
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                if (progress_active_)
                {
                    ClearProgressLine();
                    progress_active_ = false;
                }
                std::cerr << "Feature is disabled or encountered an error (stage: " << stage << ")\n";
            }
            Cleanup();
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
    }

    if (interrupted_.load())
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            if (progress_active_)
            {
                ClearProgressLine();
                progress_active_ = false;
            }
            std::cout << "\nInterrupted by user, cleaning up...\n";
        }
        Cleanup();
        return 1;
    }

    if (capture_success_ && !trace_data_.empty())
    {
        const std::string output_path = GenerateOutputPath();

        if (std::ofstream file(output_path, std::ios::binary); file.is_open())
        {
            file.write(reinterpret_cast<const char*>(trace_data_.data()), static_cast<std::streamsize>(trace_data_.size()));
            file.close();
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Trace saved to: " << output_path << '\n';
            std::cout << "Size: " << trace_data_.size() << " bytes\n";
        }
        else
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                std::cerr << "Failed to write trace file: " << output_path << '\n';
            }
            Cleanup();
            return 1;
        }
    }
    else if (!capture_success_)
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Capture failed\n";
        }
        Cleanup();
        return 1;
    }

    Cleanup();
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Done!\n";
    }
    return 0;
}

int CaptureCli::WaitForFeatureReady()
{
    RdpCaptureFeatureStage stage                = kRdpCaptureFeatureStageUnknown;
    RdpCaptureFeatureStage last_stage           = kRdpCaptureFeatureStageUnknown;
    bool                   process_info_printed = false;

    while (!interrupted_.load())
    {
        stage = fn_table_.get_feature_stage(context_, GetFeature(), 0);

        // Re-arm the connect logger whenever no app is connected, so the next
        // process to connect is logged exactly once (and not re-logged on every
        // poll while a single unsupported app stays connected).
        if (stage == kRdpCaptureFeatureStageDisconnected || stage == kRdpCaptureFeatureStageUnknown)
        {
            process_info_printed = false;
        }

        if (!process_info_printed && stage != kRdpCaptureFeatureStageDisconnected && stage != kRdpCaptureFeatureStageUnknown)
        {
            RdpCaptureProcessInfo process_info{};
            fn_table_.get_process_info(context_, &process_info);
            if (process_info.process_id != 0)
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                if (progress_active_)
                {
                    ClearProgressLine();
                    progress_active_ = false;
                }
                std::cout << "Process connected: " << process_info.process_path << " (PID: " << process_info.process_id << ")\n";
            }
            process_info_printed = true;
        }

        if (stage != last_stage)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            if (progress_active_)
            {
                ClearProgressLine();
                progress_active_ = false;
            }
            switch (stage)
            {
            case kRdpCaptureFeatureStageDisconnected:
                break;
            case kRdpCaptureFeatureStageReadyForCapture:
                std::cout << GetModeName() << " is ready for capture\n";
                break;
            case kRdpCaptureFeatureStageCapturing:
                if (config_.mode == CaptureMode::kMemoryTrace)
                {
                    std::cout << GetModeName() << " is actively tracing (type 'c' to dump)\n";
                }
                break;
            // kRdpCaptureFeatureStageApiUnsupported is explained by the
            // StatusChanged callback, emitted once when the stage transitions.
            default:
                break;
            }
            last_stage = stage;
        }

        if (stage == kRdpCaptureFeatureStageReadyForCapture)
        {
            return 0;
        }
        if (config_.mode == CaptureMode::kMemoryTrace && stage == kRdpCaptureFeatureStageCapturing)
        {
            return 0;
        }
        if (stage == kRdpCaptureFeatureStageApiUnsupported)
        {
            // Not a fatal error: the connected app uses an API this feature does
            // not support. Keep waiting for a supported app to connect. The
            // "does not support" message is emitted by the StatusChanged
            // callback; the connect logger re-arms when the app disconnects.
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
            continue;
        }
        if (stage < 0)
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                if (progress_active_)
                {
                    ClearProgressLine();
                    progress_active_ = false;
                }
                std::cerr << "Feature is disabled or encountered an error (stage: " << stage << ")\n";
            }
            Cleanup();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        if (progress_active_)
        {
            ClearProgressLine();
            progress_active_ = false;
        }
        std::cout << "\nInterrupted by user, cleaning up...\n";
    }
    Cleanup();
    return 1;
}

int CaptureCli::RunManualCaptureLoop()
{
    if (const int wait_result = WaitForFeatureReady(); wait_result != 0)
    {
        return wait_result;
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << '\n';
        std::cout << "Commands:\n";
        if (config_.mode == CaptureMode::kMemoryTrace)
        {
            std::cout << "  c, capture              - Dump the current memory trace to file\n";
            std::cout << "  s, snapshot [name]      - Insert a snapshot marker\n";
        }
        else if (config_.mode == CaptureMode::kProfiling)
        {
            std::cout << "  c, capture              - Trigger a capture\n";
            std::cout << "  mode [value[:N]]        - View or set capture mode (default, frame, draw, dispatch)\n";
            std::cout << "                            N = render op count for draw/dispatch mode\n";
        }
        else
        {
            std::cout << "  c, capture  - Trigger a capture\n";
        }
        std::cout << "  q, quit     - Exit the application\n";
        std::cout << '\n';
    }

    while (!interrupted_.load())
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            if (progress_active_)
            {
                ClearProgressLine();
                progress_active_ = false;
            }
            std::cout << "> " << std::flush;
        }

        std::string command;
        if (!std::getline(std::cin, command))
        {
            break;
        }

        const size_t start = command.find_first_not_of(" \t\r\n");
        const size_t end   = command.find_last_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            continue;
        }
        command = command.substr(start, end - start + 1);

        // Preserve original (case-sensitive) input before lowercasing so that a
        // snapshot name typed by the user keeps its original casing.
        const std::string original_command = command;

        std::ranges::transform(command, command.begin(), ToLowerChar);

        // For memory trace mode, detect "s [name]" / "snapshot [name]" before the
        // shared command dispatch below.  Extract the name from the pre-lowercased
        // input so its casing is preserved.
        bool        is_snapshot_cmd = false;
        std::string snapshot_name;
        if (config_.mode == CaptureMode::kMemoryTrace)
        {
            const size_t      separator_pos = command.find_first_of(" \t");
            const std::string keyword       = (separator_pos != std::string::npos) ? command.substr(0, separator_pos) : command;
            if (keyword == "s" || keyword == "snapshot")
            {
                is_snapshot_cmd = true;
                if (separator_pos != std::string::npos)
                {
                    const size_t name_start = original_command.find_first_not_of(" \t", separator_pos);
                    snapshot_name           = (name_start != std::string::npos) ? original_command.substr(name_start) : "";
                }
            }
        }

        if (command == "q" || command == "quit" || command == "exit")
        {
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                std::cout << "Exiting...\n";
            }
            Cleanup();
            return 0;
        }
        if (command == "c" || command == "capture")
        {
            PerformCapture();
        }
        else if (config_.mode == CaptureMode::kProfiling && (command.starts_with("mode") && (command.size() == 4 || command[4] == ' ' || command[4] == '\t')))
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            const size_t                      arg_start = command.find_first_not_of(" \t", 4);
            if (arg_start == std::string::npos)
            {
                std::cout << "Capture mode: " << RgpCaptureModeName(config_.rgp_capture_mode);
                if (config_.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDraw || config_.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDispatch)
                {
                    std::cout << ":" << config_.rgp_render_op_count;
                }
                std::cout << '\n';
            }
            else
            {
                const std::string mode_arg  = command.substr(arg_start);
                const size_t      colon_pos = mode_arg.find(':');
                const std::string mode_str  = (colon_pos != std::string::npos) ? mode_arg.substr(0, colon_pos) : mode_arg;

                CaptureConfig temp_config = config_;
                if (!ParseRgpCaptureMode(mode_str, temp_config))
                {
                    // ParseRgpCaptureMode already printed the error.
                }
                else if (colon_pos != std::string::npos)
                {
                    const bool mode_uses_render_ops = temp_config.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDraw ||
                                                      temp_config.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDispatch;
                    if (!mode_uses_render_ops)
                    {
                        std::cerr << "Render op count is not applicable for '" << mode_str << "' mode\n";
                    }
                    else
                    {
                        const auto parsed = ParseNumber<uint32_t>(mode_arg.substr(colon_pos + 1));
                        if (!parsed || *parsed == 0)
                        {
                            std::cerr << "Invalid render op count. Must be a positive integer.\n";
                        }
                        else
                        {
                            config_.rgp_capture_mode    = temp_config.rgp_capture_mode;
                            config_.rgp_render_op_count = *parsed;
                            ApplyProfilingParams();
                            std::cout << "Capture mode set to: " << RgpCaptureModeName(config_.rgp_capture_mode) << ":" << config_.rgp_render_op_count << '\n';
                        }
                    }
                }
                else
                {
                    config_.rgp_capture_mode = temp_config.rgp_capture_mode;
                    ApplyProfilingParams();
                    std::cout << "Capture mode set to: " << RgpCaptureModeName(config_.rgp_capture_mode) << '\n';
                }
            }
        }
        else if (is_snapshot_cmd)
        {
            const RdpCaptureFeatureStage stage = fn_table_.get_feature_stage(context_, GetFeature(), 0);
            if (stage != kRdpCaptureFeatureStageCapturing)
            {
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                std::cerr << "Memory trace is not actively tracing (stage: " << stage << ")\n";
            }
            else
            {
                const RdpCaptureResult res = fn_table_.memory_trace.insert_snapshot(context_, RDP_CAPTURE_API_FIRST_CONNECTION_ID, snapshot_name.c_str());
                const std::lock_guard<std::mutex> lock(g_output_mutex);
                if (res != kRdpCaptureResultSuccess)
                {
                    std::cerr << "Failed to insert snapshot (result: " << res << ")\n";
                }
                else
                {
                    std::cout << "Snapshot inserted" << (snapshot_name.empty() ? "" : (": " + snapshot_name)) << '\n';
                }
            }
        }
        else if (!command.empty())
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Unknown command: " << command << '\n';
            if (config_.mode == CaptureMode::kMemoryTrace)
            {
                std::cout << "Type 'c' to dump trace, 's [name]' to insert snapshot, 'q' to quit\n";
            }
            else if (config_.mode == CaptureMode::kProfiling)
            {
                std::cout << "Type 'c' to capture, 'mode [value[:N]]' to set capture mode, 'q' to quit\n";
            }
            else
            {
                std::cout << "Type 'c' to capture, 'q' to quit\n";
            }
        }
    }

    if (interrupted_.load())
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        if (progress_active_)
        {
            ClearProgressLine();
            progress_active_ = false;
        }
        std::cout << "\nInterrupted by user, cleaning up...\n";
    }

    Cleanup();
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Done!\n";
    }
    return 0;
}

int CaptureCli::PerformCapture()
{
    const RdpCaptureFeatureStage stage = fn_table_.get_feature_stage(context_, GetFeature(), 0);

    if (config_.mode == CaptureMode::kMemoryTrace)
    {
        if (stage != kRdpCaptureFeatureStageCapturing)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Memory trace is not actively tracing (stage: " << stage << ")\n";
            return -1;
        }
    }
    else
    {
        if (stage != kRdpCaptureFeatureStageReadyForCapture)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Feature is not ready for capture (stage: " << stage << ")\n";
            if (stage == kRdpCaptureFeatureStageCapturing)
            {
                std::cout << "A capture is already in progress...\n";
            }
            return -1;
        }
    }

    if (config_.mode == CaptureMode::kRaytracing && config_.rra_delay_ms > 0)
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Delaying " << GetModeName() << " capture by " << config_.rra_delay_ms << " ms...\n";
        }
        // Poll in small slices so Ctrl+C remains responsive during the delay.
        constexpr uint32_t kDelaySliceMs = 50;
        uint32_t           remaining_ms  = config_.rra_delay_ms;
        while (remaining_ms > 0 && !interrupted_.load())
        {
            const uint32_t slice = std::min(remaining_ms, kDelaySliceMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            remaining_ms -= slice;
        }
        if (interrupted_.load())
        {
            return -1;
        }
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Triggering " << GetModeName() << " capture...\n";
    }

    if (const std::string trigger_error = TriggerCaptureForMode(config_.mode, fn_table_, context_); !trigger_error.empty())
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << trigger_error << '\n';
        return -1;
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Capture triggered, waiting for completion...\n";
    }

    while (!capture_complete_.load() && !interrupted_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollingIntervalMs));
    }

    if (interrupted_.load())
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            if (progress_active_)
            {
                ClearProgressLine();
                progress_active_ = false;
            }
            std::cout << "\nInterrupted by user, cleaning up...\n";
        }
        Cleanup();
        return 1;
    }

    SaveAndCleanupCapture();
    return 0;
}

bool CaptureCli::WarnIfNoAccelerationStructures() const
{
    // This chunk was renamed in a driver update; both are checked for backwards compatibility.
    // SEE: rra_trace_validator.cpp (kAccelStructNameOld / kAccelStructNameNew).
    static constexpr const char* kAccelStructNameOld = "RawAccelStruc";
    static constexpr const char* kAccelStructNameNew = "RawAccelStruct";

    rdfStream*    stream     = nullptr;
    rdfChunkFile* chunk_file = nullptr;

    const auto cleanup = [&]() {
        if (chunk_file != nullptr)
        {
            rdfChunkFileClose(&chunk_file);
        }
        if (stream != nullptr)
        {
            rdfStreamClose(&stream);
        }
    };

    if (rdfStreamFromReadOnlyMemory(static_cast<int64_t>(trace_data_.size()), trace_data_.data(), &stream) != rdfResultOk)
    {
        cleanup();
        return false;
    }

    if (rdfChunkFileOpenStream(stream, &chunk_file) != rdfResultOk)
    {
        cleanup();
        return false;
    }

    int64_t    old_count = 0;
    int64_t    new_count = 0;
    const bool old_ok    = rdfChunkFileGetChunkCount(chunk_file, kAccelStructNameOld, &old_count) == rdfResultOk;
    const bool new_ok    = rdfChunkFileGetChunkCount(chunk_file, kAccelStructNameNew, &new_count) == rdfResultOk;
    cleanup();

    // If either query failed (e.g. corrupt RDF), don't warn — let the capture stand.
    if (!old_ok || !new_ok)
    {
        return false;
    }

    if (old_count == 0 && new_count == 0)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "[WARNING] No acceleration structures captured.\n"
                  << "          Please ensure that the application has ray tracing enabled and that ray tracing\n"
                  << "          is taking place at the time of capture. Also ensure that top-level acceleration\n"
                  << "          structures are built each frame and not destroyed, cleared, or reused in that\n"
                  << "          same frame. A few more capture attempts may be necessary in some instances.\n";
        return true;
    }

    return false;
}

void CaptureCli::SaveAndCleanupCapture()
{
    if (capture_success_ && !trace_data_.empty())
    {
        // For RRA, validate before writing: if there are no acceleration structures the
        // trace is unusable and should not be saved (mirrors GUI behaviour).
        if (config_.mode == CaptureMode::kRaytracing && WarnIfNoAccelerationStructures())
        {
            trace_data_.clear();
            capture_complete_.store(false);
            capture_success_ = false;
            return;
        }

        const std::string output_path = GenerateOutputPath();
        if (std::ofstream file(output_path, std::ios::binary); file.is_open())
        {
            file.write(reinterpret_cast<const char*>(trace_data_.data()), static_cast<std::streamsize>(trace_data_.size()));
            file.close();
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Trace saved to: " << output_path << '\n';
            std::cout << "Size: " << trace_data_.size() << " bytes\n";
        }
        else
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Failed to write trace file: " << output_path << '\n';
        }

        trace_data_.clear();
        capture_complete_.store(false);
        capture_success_ = false;
    }
    else if (!capture_success_)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "Capture failed\n";
        capture_complete_.store(false);
    }
}

int CaptureCli::RunClocksMode() const
{
    if (fn_table_.get_system_info == nullptr || fn_table_.free_system_info == nullptr)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "System info is not supported by this capture API version.\n";
        return 1;
    }

    // Get the list of GPUs (via aggregated system info; we only consume the GPUs section).
    RdpCaptureSystemInfo system_info{};
    if (fn_table_.get_system_info(context_, &system_info) != kRdpCaptureResultSuccess)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "Failed to query system info\n";
        return 1;
    }

    const uint64_t       num_gpus = system_info.num_gpus;
    const RdpCaptureGpu* gpus     = system_info.gpus;

    if (num_gpus == 0)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "No GPUs found\n";
        fn_table_.free_system_info(&system_info);
        return 1;
    }

    // Validate GPU index
    if (config_.gpu_index >= num_gpus)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "Invalid GPU index " << config_.gpu_index << " (found " << num_gpus << " GPU(s))\n";
        fn_table_.free_system_info(&system_info);
        return 1;
    }

    // Display GPU information
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Found " << num_gpus << " GPU(s):\n";
        for (uint64_t i = 0; i < num_gpus; ++i)
        {
            std::cout << "  [" << i << "] " << gpus[i].name << '\n';
        }
        std::cout << '\n';
        std::cout << "Using GPU [" << config_.gpu_index << "]: " << gpus[config_.gpu_index].name << '\n';
    }

    const uint64_t gpu_index = config_.gpu_index;
    fn_table_.free_system_info(&system_info);

    // Query current clock mode
    RdpCaptureGpuClockMode current_mode = kRdpCaptureGpuClocksModeUnknown;
    RdpCaptureResult       result       = fn_table_.query_gpu_current_clock_mode(context_, gpu_index, &current_mode);
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        if (result == kRdpCaptureResultSuccess)
        {
            std::cout << "Current clock mode: " << GetClockModeName(current_mode) << '\n';
        }
        else
        {
            std::cerr << "Failed to query current clock mode (result: " << result << ")\n";
        }
    }

    // Display available clock modes
    RdpCaptureGpuClockModeDetails* modes     = nullptr;
    uint64_t                       num_modes = 0;
    fn_table_.get_gpu_clock_modes(context_, gpu_index, &modes, &num_modes);

    if (num_modes > 0)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Available clock modes:\n";
        for (uint64_t i = 0; i < num_modes; ++i)
        {
            std::cout << "  " << GetClockModeName(modes[i].mode);
            std::cout << " (GPU: " << modes[i].min_gpu_freq / kHzPerMhz << "-" << modes[i].max_gpu_freq / kHzPerMhz << " MHz";
            std::cout << ", Mem: " << modes[i].min_mem_freq / kHzPerMhz << "-" << modes[i].max_mem_freq / kHzPerMhz << " MHz)";
            std::cout << '\n';
        }
        fn_table_.free(modes);
    }

    // Set clock mode if specified
    if (config_.clock_mode != kRdpCaptureGpuClocksModeUnknown)
    {
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << '\n' << "Setting clock mode to: " << GetClockModeName(config_.clock_mode) << "...\n";
        }
        result = fn_table_.set_gpu_current_clock_mode(context_, gpu_index, config_.clock_mode);
        if (result != kRdpCaptureResultSuccess)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Failed to set clock mode (result: " << result << ")\n";
            return 1;
        }

        // Verify the change
        RdpCaptureGpuClockMode new_mode = kRdpCaptureGpuClocksModeUnknown;
        result                          = fn_table_.query_gpu_current_clock_mode(context_, gpu_index, &new_mode);
        if (result == kRdpCaptureResultSuccess)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "Clock mode set to: " << GetClockModeName(new_mode) << '\n';
        }
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Done!\n";
    }
    return 0;
}

bool CaptureCli::Initialize()
{
    // Get function table
    RdpCaptureResult result = RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &fn_table_);
    if (result != kRdpCaptureResultSuccess)
    {
        std::cerr << "Failed to get capture API function table\n";
        return false;
    }

    // Initialize context
    RdpCaptureContextInitParams init_params{};
    init_params.app_filter.filter    = AppFilterWrapper;
    init_params.app_filter.user_data = this;

    if (config_.verbose)
    {
        init_params.log_callback.log       = LogCallbackWrapper;
        init_params.log_callback.user_data = this;
    }

    if (!config_.remote_host.empty())
    {
        init_params.remote_connection.hostname = config_.remote_host.c_str();
        init_params.remote_connection.port     = config_.remote_port;
    }

    result = fn_table_.initialize(&init_params, &context_);
    if (result != kRdpCaptureResultSuccess)
    {
        std::cerr << "Failed to initialize capture context\n";
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Capture API initialized successfully\n";
    }
    return true;
}

bool CaptureCli::EnableFeature()
{
    RdpCaptureFeatureEnableParams enable_params{};
    enable_params.feature                                = GetFeature();
    enable_params.trace_finished_callback.trace_finished = TraceFinishedWrapper;
    enable_params.trace_finished_callback.user_data      = this;
    enable_params.status_callback.status_changed         = StatusCallbackWrapper;
    enable_params.status_callback.user_data              = this;
    enable_params.progress_callback.progress_updated     = ProgressCallbackWrapper;
    enable_params.progress_callback.user_data            = this;

    // Backing storage for crash-analysis PDB search-path C-string array; kept
    // alive until enable_feature returns.
    std::vector<const char*> rgd_pdb_path_ptrs;

    switch (config_.mode)
    {
    case CaptureMode::kProfiling:
        if (config_.enable_shader_instrumentation)
        {
            enable_params.body.profiling.flags |= kRdpCaptureProfilingEnableParamFlagEnableShaderInstrumentation;
        }
        ConfigureProfilingAutoCapture(enable_params);
        break;

    case CaptureMode::kRaytracing:
        if (config_.rra_enable_marker_capture)
        {
            enable_params.body.raytracing.flags |= kRdpCaptureRaytracingEnableParamFlagEnableMarkerCapture;
            enable_params.body.raytracing.marker_begin_string = config_.rra_marker_begin.c_str();
            enable_params.body.raytracing.marker_end_string   = config_.rra_marker_end.c_str();
        }
        break;

    case CaptureMode::kMemoryTrace:
    case CaptureMode::kClocks:
        // Default parameters - no additional setup needed
        break;

    case CaptureMode::kCrashAnalysis:
    {
        uint32_t crash_flags = 0;
        if (config_.enable_enhanced_crash_analysis)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagEnableEnhancedCrashAnalysis;
        }
        // Note: the GenerateTextSummary / GenerateJsonSummary flag bits intentionally are *not*
        // set here. The Capture API's registered summary generator is a no-op, so propagating
        // these would only enqueue a failed processing pass on trace completion. The CLI invokes
        // rgd[.exe] directly after the dump is written instead -- see GenerateCrashSummaries().
        if (config_.rgd_show_marker_source)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagShowMarkerSource;
        }
        if (config_.rgd_expand_markers)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagExpandMarkers;
        }
        if (config_.rgd_pdb_include_subfolders)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagPdbIncludeSubfolders;
        }
        if (config_.rgd_collect_wave_sgprs)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveSgprs;
        }
        if (config_.rgd_collect_wave_vgprs)
        {
            crash_flags |= kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveVgprs;
        }
        enable_params.body.crash_analysis.flags = crash_flags;

        rgd_pdb_path_ptrs.reserve(config_.rgd_pdb_search_paths.size());
        for (const std::string& path : config_.rgd_pdb_search_paths)
        {
            rgd_pdb_path_ptrs.push_back(path.c_str());
        }
        enable_params.body.crash_analysis.pdb_search_paths     = rgd_pdb_path_ptrs.empty() ? nullptr : rgd_pdb_path_ptrs.data();
        enable_params.body.crash_analysis.num_pdb_search_paths = rgd_pdb_path_ptrs.size();
        break;
    }
    }

    if (const RdpCaptureResult result = fn_table_.enable_feature(context_, &enable_params); result != kRdpCaptureResultSuccess)
    {
        if (result == kRdpCaptureResultUnsupported && config_.rra_enable_marker_capture)
        {
            const std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cerr << "Error: Marker-based capture requires driver version 26.20 or newer.\n";
        }
        return false;
    }

    ApplyProfilingParams();
    ApplyRaytracingParams();

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << GetModeName() << " feature enabled";
        if (config_.auto_capture_mode != AutoCaptureMode::kNone)
        {
            std::cout << " (auto-capture enabled)";
        }
        std::cout << '\n';
    }
    return true;
}

void CaptureCli::ApplyProfilingParams() const
{
    if (config_.mode != CaptureMode::kProfiling)
    {
        return;
    }

    RdpCaptureProfilingParams profiling_params{};
    fn_table_.profiling.get_default_params(&profiling_params);

    if (config_.enable_instruction_tracing)
    {
        profiling_params.flags |= kRdpCaptureProfilingParamFlagEnableInstructionTracing;
    }
    if (config_.enable_counter_collection)
    {
        profiling_params.flags |= kRdpCaptureProfilingParamFlagEnableCounterCollection;
    }
    profiling_params.sqtt_buffer_size = config_.sqtt_buffer_size;
    profiling_params.capture_mode     = static_cast<RdpCaptureProfilingCaptureMode>(config_.rgp_capture_mode);
    profiling_params.render_op_count  = config_.rgp_render_op_count;

    fn_table_.profiling.set_params(context_, &profiling_params);
}

void CaptureCli::ApplyRaytracingParams() const
{
    if (config_.mode != CaptureMode::kRaytracing)
    {
        return;
    }

    RdpCaptureRaytracingParams raytracing_params{};
    fn_table_.raytracing.get_default_params(&raytracing_params);

    raytracing_params.ray_history_buffer_size =
        config_.rra_collect_ray_dispatch_data ? config_.rra_ray_history_buffer_size : kRdpCaptureRayHistoryBufferSizeRayHistoryDisabled;

    if (config_.rra_enable_marker_capture)
    {
        raytracing_params.enable_marker_capture = 1;
        raytracing_params.marker_begin_string   = config_.rra_marker_begin.c_str();
        raytracing_params.marker_end_string     = config_.rra_marker_end.c_str();
    }

    fn_table_.raytracing.set_params(context_, &raytracing_params);
}

void CaptureCli::ConfigureProfilingAutoCapture(RdpCaptureFeatureEnableParams& enable_params) const
{
    switch (config_.auto_capture_mode)
    {
    case AutoCaptureMode::kFrameIndex:
        enable_params.body.profiling.flags |= kRdpCaptureProfilingEnableParamFlagUseFrameIndexCapture;
        enable_params.body.profiling.frame_capture_index = config_.frame_capture_index;
        break;
    case AutoCaptureMode::kDispatchIndex:
        enable_params.body.profiling.flags |= kRdpCaptureProfilingEnableParamFlagUseDispatchIndexCapture;
        enable_params.body.profiling.dispatch_start_index      = config_.dispatch_start_index;
        enable_params.body.profiling.dispatch_count            = config_.dispatch_count;
        enable_params.body.profiling.dispatch_capture_delay_ms = config_.dispatch_capture_delay_ms;
        break;
    case AutoCaptureMode::kNone:
    default:
        break;
    }
}

void CaptureCli::Cleanup()
{
    if (context_ != nullptr)
    {
        if (config_.mode != CaptureMode::kClocks)
        {
            fn_table_.disable_feature(context_, GetFeature());
        }
        fn_table_.destroy(context_);
        context_ = nullptr;
    }
}

bool CaptureCli::AppFilter(const RdpCaptureProcessInfo* process_info, const RdpCaptureGpuApi api)
{
    // Filter by supported APIs (Vulkan, DX12, OpenCL, HIP)
    if (api != kRdpCaptureGpuApiVulkan && api != kRdpCaptureGpuApiDirectX12 && api != kRdpCaptureGpuApiOpenCl && api != kRdpCaptureGpuApiHip)
    {
        return false;
    }

    // Apply process filter if specified
    if (!config_.process_filter.empty())
    {
        const std::string process_path(process_info->process_path);
        // Case-insensitive substring search
        std::string filter_lower = config_.process_filter;
        std::string path_lower   = process_path;
        std::ranges::transform(filter_lower, filter_lower.begin(), ToLowerChar);
        std::ranges::transform(path_lower, path_lower.begin(), ToLowerChar);

        if (path_lower.find(filter_lower) == std::string::npos)
        {
            return false;
        }
    }

    // Store the connected process name (executable stem without extension) for output file naming.
    // Lock covers both the shared string write and the cout to prevent races with callback threads.
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        connected_process_name_ = std::filesystem::path(process_info->process_path).stem().string();
        std::cout << "Connecting to process: " << process_info->process_path << " (PID: " << process_info->process_id << ")\n";
    }
    return true;
}

void CaptureCli::LogCallback(const RdpCaptureLogLevel level, const char* source, const uint32_t process_id, const char* msg)
{
    const char* level_str = nullptr;
    switch (level)
    {
    case kRdpCaptureLogLevelVerbose:
        level_str = "VERBOSE";
        break;
    case kRdpCaptureLogLevelInfo:
        level_str = "INFO";
        break;
    case kRdpCaptureLogLevelWarning:
        level_str = "WARNING";
        break;
    case kRdpCaptureLogLevelError:
        level_str = "ERROR";
        break;
    }

    const std::lock_guard<std::mutex> lock(g_output_mutex);
    std::cout << "[" << (level_str != nullptr ? level_str : "UNKNOWN") << "] [" << source << "] (PID:" << process_id << ") " << msg << '\n';
}

void CaptureCli::TraceFinished([[maybe_unused]] RdpCaptureFeature         feature,
                               [[maybe_unused]] RdpCaptureApiConnectionId connection,
                               const RdpCaptureResult                     result,
                               const std::span<const uint8_t>             data)
{
    const std::lock_guard<std::mutex> lock(g_output_mutex);

    if (progress_active_)
    {
        ClearProgressLine();
        progress_active_ = false;
    }

    if (result == kRdpCaptureResultSuccess)
    {
        std::cout << GetModeName() << " trace completed successfully! Size: " << data.size() << " bytes\n";
        capture_success_ = true;
        if (!data.empty())
        {
            trace_data_.assign(data.begin(), data.end());
        }
    }
    else
    {
        std::cerr << GetModeName() << " trace failed with result: " << result << '\n';
        capture_success_ = false;
    }

    capture_complete_.store(true);
}

void CaptureCli::StatusChanged([[maybe_unused]] const RdpCaptureFeature       feature,
                               const RdpCaptureDetailedStage                  new_stage,
                               [[maybe_unused]] const RdpCaptureDetailedStage old_stage)
{
    const std::lock_guard<std::mutex> lock(g_output_mutex);

    if (progress_active_)
    {
        ClearProgressLine();
        progress_active_ = false;
    }
    std::cout << "[Status] " << GetDetailedStageName(new_stage) << '\n';

    // The connected app uses an API this feature does not support. This is not a
    // fatal error. Emitting it here (on the stage transition) explains it the
    // same way for every mode and capture loop without per-poll repetition.
    if (new_stage == kRdpCaptureDetailedStageApiUnsupported)
    {
        std::cout << "Connected process does not support " << GetModeName() << ", waiting for next connection...\n";
    }
}

void CaptureCli::ProgressUpdated(const RdpCaptureProgressInfo* info)
{
    if (info == nullptr)
    {
        return;
    }

    std::string label;
    if (info->progress_text != nullptr && info->progress_text[0] != '\0')
    {
        label = info->progress_text;
    }
    else if (info->total_bytes_to_dump > 0)
    {
        const double       dumped_mb = static_cast<double>(info->num_bytes_dumped) / (1024.0 * 1024.0);
        const double       total_mb  = static_cast<double>(info->total_bytes_to_dump) / (1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << dumped_mb << " / " << total_mb << " MB";
        label = oss.str();
    }

    const std::lock_guard<std::mutex> lock(g_output_mutex);
    progress_active_ = true;
    RenderProgressBar(info->progress, label);
}

void CaptureCli::RenderProgressBar(const float progress, const std::string& label)
{
    constexpr int kBarWidth = 40;

    const float clamped = std::max(0.0f, std::min(1.0f, progress));
    const int   filled  = static_cast<int>(clamped * kBarWidth);
    const int   percent = static_cast<int>(clamped * 100.0f);

    std::ostringstream bar;
    bar << "\r  [";
    for (int i = 0; i < kBarWidth; ++i)
    {
        if (i < filled)
        {
            bar << '#';
        }
        else if (i == filled)
        {
            bar << '>';
        }
        else
        {
            bar << ' ';
        }
    }
    bar << "] " << std::setw(3) << percent << "%";

    if (!label.empty())
    {
        bar << "  " << label;
    }

    // Pad with spaces to clear any leftover characters from previous longer lines
    bar << "   ";

    std::cout << bar.str() << std::flush;
}

std::string CaptureCli::GetDetailedStageName(const RdpCaptureDetailedStage stage)
{
    switch (stage)
    {
    case kRdpCaptureDetailedStageUnknown:
        return "Unknown";
    case kRdpCaptureDetailedStageDisconnected:
        return "Disconnected";
    case kRdpCaptureDetailedStageReadyForCapture:
        return "Ready for capture";
    case kRdpCaptureDetailedStageWaitingToBeginCapture:
        return "Waiting to begin capture";
    case kRdpCaptureDetailedStageCapturing:
        return "Capturing";
    case kRdpCaptureDetailedStageDumping:
        return "Dumping trace data";
    case kRdpCaptureDetailedStageProcessing:
        return "Processing trace data";
    case kRdpCaptureDetailedStageDone:
        return "Done";
    case kRdpCaptureDetailedStageEncounteredError:
        return "Error";
    case kRdpCaptureDetailedStageBusy:
        return "Busy";
    case kRdpCaptureDetailedStageDisabled:
        return "Disabled";
    case kRdpCaptureDetailedStageDisabledFromError:
        return "Disabled (error)";
    case kRdpCaptureDetailedStageHardwareUnsupported:
        return "Hardware unsupported";
    case kRdpCaptureDetailedStageOsUnsupported:
        return "OS unsupported";
    case kRdpCaptureDetailedStageDriverUnsupported:
        return "Driver unsupported";
    case kRdpCaptureDetailedStageApiUnsupported:
        return "API unsupported";
    default:
        return "Unknown (" + std::to_string(stage) + ")";
    }
}

void CaptureCli::ClearProgressLine()
{
    std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;
}

std::string CaptureCli::GetFileExtension() const
{
    switch (config_.mode)
    {
    case CaptureMode::kProfiling:
    case CaptureMode::kClocks:
        return ".rgp";
    case CaptureMode::kRaytracing:
        return ".rra";
    case CaptureMode::kMemoryTrace:
        return ".rmv";
    case CaptureMode::kCrashAnalysis:
        return ".rgd";
    }
    return ".rgp";
}

std::string CaptureCli::GenerateOutputPath() const
{
    // If user specified an output path, use it (ensuring correct extension)
    if (!config_.output_path.empty())
    {
        std::filesystem::path output_path(config_.output_path);
        const std::string     expected_ext = GetFileExtension();

        // If no extension or wrong extension, add/replace with correct one
        if (!output_path.has_extension())
        {
            output_path += expected_ext;
        }
        else
        {
            std::string ext_lower      = output_path.extension().string();
            std::string expected_lower = expected_ext;
            std::ranges::transform(ext_lower, ext_lower.begin(), ToLowerChar);
            std::ranges::transform(expected_lower, expected_lower.begin(), ToLowerChar);

            if (ext_lower != expected_lower)
            {
                output_path.replace_extension(expected_ext);
            }
        }

        return output_path.string();
    }

    // Generate timestamp-based filename
    const auto now        = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm    tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S");
    const std::string timestamp = oss.str();
    const std::string extension = GetFileExtension();

    // Snapshot the connected process name under the lock, since AppFilter may update it from an API callback thread.
    std::string process_name;
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        process_name = connected_process_name_;
    }

    // Use the connected application name as the prefix, falling back to mode name
    std::string prefix;
    if (!process_name.empty())
    {
        prefix = process_name;
    }
    else
    {
        switch (config_.mode)
        {
        case CaptureMode::kProfiling:
            prefix = "profile";
            break;
        case CaptureMode::kRaytracing:
            prefix = "raytracing";
            break;
        case CaptureMode::kMemoryTrace:
            prefix = "memory";
            break;
        case CaptureMode::kCrashAnalysis:
            prefix = "crash";
            break;
        case CaptureMode::kClocks:
            prefix = "clocks";
            break;
        }
    }

    return prefix + "_" + timestamp + extension;
}

RdpCaptureFeature CaptureCli::GetFeature() const
{
    switch (config_.mode)
    {
    case CaptureMode::kProfiling:
    case CaptureMode::kClocks:
        return kRdpCaptureFeatureProfiling;
    case CaptureMode::kRaytracing:
        return kRdpCaptureFeatureRaytracing;
    case CaptureMode::kMemoryTrace:
        return kRdpCaptureFeatureMemoryTrace;
    case CaptureMode::kCrashAnalysis:
        return kRdpCaptureFeatureCrashAnalysis;
    }
    return kRdpCaptureFeatureProfiling;
}

std::string CaptureCli::GetModeName() const
{
    switch (config_.mode)
    {
    case CaptureMode::kProfiling:
        return "Profiling (RGP)";
    case CaptureMode::kRaytracing:
        return "Raytracing (RRA)";
    case CaptureMode::kMemoryTrace:
        return "Memory Trace (RMV)";
    case CaptureMode::kCrashAnalysis:
        return "Crash Analysis (RGD)";
    case CaptureMode::kClocks:
        return "Clocks";
    }
    return "Unknown";
}

std::string CaptureCli::GetClockModeName(const RdpCaptureGpuClockMode mode)
{
    switch (mode)
    {
    case kRdpCaptureGpuClocksModeNormal:
        return "Normal";
    case kRdpCaptureGpuClocksModeStable:
        return "Stable";
    case kRdpCaptureGpuClocksModePeak:
        return "Peak";
    case kRdpCaptureGpuClocksModeUnknown:
    default:
        return "Unknown";
    }
}

void CaptureCli::ListBlocklist() const
{
    char**   entries     = nullptr;
    uint64_t num_entries = 0;
    fn_table_.blocklist.get_entries(context_, &entries, &num_entries);

    std::cout << "Active blocklist entries (" << num_entries << "):\n";
    std::cout << std::string(kSeparatorWidth, '-') << '\n';

    for (uint64_t i = 0; i < num_entries; ++i)
    {
        std::cout << "  " << entries[i] << '\n';
    }

    fn_table_.blocklist.free_entries(entries, num_entries);
}

void CaptureCli::WarnIfEtwNotConfigured() const
{
#if defined(_WIN32)
    if (fn_table_.get_system_info == nullptr || fn_table_.free_system_info == nullptr)
    {
        return;
    }
    RdpCaptureSystemInfo system_info{};
    if (fn_table_.get_system_info(context_, &system_info) != kRdpCaptureResultSuccess)
    {
        return;
    }
    if (system_info.os.etw_needs_script)
    {
        std::cerr << "[WARNING] Event Tracing for Windows (ETW) is not fully configured.\n"
                  << "          DX12 Signal/Wait capture and memory trace resource naming may not function.\n"
                  << "          Please run scripts/AddUserToGroup.bat as Administrator to enable ETW.\n";
    }
    fn_table_.free_system_info(&system_info);
#endif
}

void CaptureCli::PrintSystemInfo() const
{
    if (fn_table_.get_system_info == nullptr || fn_table_.free_system_info == nullptr)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "System info is not supported by this capture API version.\n";
        return;
    }

    RdpCaptureSystemInfo system_info{};
    if (fn_table_.get_system_info(context_, &system_info) != kRdpCaptureResultSuccess)
    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cerr << "Failed to query system info.\n";
        return;
    }

    // Build the full output into a local stream and flush it once under the
    // output mutex so it cannot interleave with concurrent callback output.
    std::ostringstream oss;
    oss << "System info:\n";
    oss << std::string(kSeparatorWidth, '-') << '\n';

    // Helpers below mirror the formatters in source/modules/common/src/formatting.cpp
    // (Formatting::FormatBytesPow2 / FormatBandwidth / FormatHertz / HzToMHz /
    // MHzToGHz) so the CLI presents values in the same units and units-suffixes
    // as the RDP GUI's System Info pane.

    // Auto-scaling formatter that picks the largest denomination whose
    // upper-bound exceeds the value.  Lowest denomination is always emitted
    // without decimals (matching Formatting::Format).
    auto format_scaled = [](uint64_t value, uint64_t base, std::initializer_list<const char*> labels, int num_decimals) {
        const auto*        first = labels.begin();
        const std::size_t  count = labels.size();
        uint64_t           upper = 1;
        std::ostringstream out;
        out << std::fixed;
        for (std::size_t idx = 0; idx < count; ++idx)
        {
            upper *= base;
            if (value < upper || idx == count - 1)
            {
                const double unit_value = static_cast<double>(value) / static_cast<double>(upper / base);
                out << std::setprecision(idx == 0 ? 0 : num_decimals) << unit_value << ' ' << first[idx];
                return out.str();
            }
        }
        return std::string{};
    };

    // GUI's Formatting::FormatBytesPow2: divisor 1024 with B/KB/MB/GB/TB/PB
    // suffixes (note: SI labels with binary math, kept for parity with GUI).
    auto format_bytes_pow2 = [&](uint64_t bytes, int num_decimals = 3) {
        return format_scaled(bytes, 1024, {"B", "KB", "MB", "GB", "TB", "PB"}, num_decimals);
    };

    // GUI's Formatting::FormatBandwidth: divisor 1000 with B/s..PB/s suffixes.
    auto format_bandwidth = [&](uint64_t bytes_per_second, int num_decimals = 0) {
        return format_scaled(bytes_per_second, 1000, {"B/s", "KB/s", "MB/s", "GB/s", "TB/s", "PB/s"}, num_decimals);
    };

    // GUI's Formatting::FormatHertz: divisor 1000 with Hz/KHz/MHz/GHz, default
    // 0 decimals.  Used for the auto-scaled (min) clock fields.
    auto format_hertz = [&](uint64_t hertz, int num_decimals = 0) { return format_scaled(hertz, 1000, {"Hz", "KHz", "MHz", "GHz"}, num_decimals); };

    // GUI's Formatting::HzToMHz: like FormatHertz but capped at MHz so values
    // in the GHz range still display as MHz.  Used for the (max) clock fields
    // to match the GUI's exact unit choice.
    auto hz_to_mhz = [&](uint64_t hertz) { return format_scaled(hertz, 1000, {"Hz", "KHz", "MHz"}, 0); };

    // GUI's Formatting::MHzToGHz: input in MHz, divisor 10, suffixes
    // Hz/KHz/MHz/GHz, 2 decimals.  Used for CPU speed.
    auto mhz_to_ghz = [&](uint64_t mhz) { return format_scaled(mhz, 10, {"Hz", "KHz", "MHz", "GHz"}, 2); };

    auto format_hex = [](uint32_t value) {
        // Matches the GUI's SystemInfoModel::FormatHex: uppercase hex with no
        // "0x" prefix and no zero padding (QString::number(value, 16).toUpper()).
        std::ostringstream out;
        out << std::uppercase << std::hex << value;
        return out.str();
    };

    // Format a YYMMDD date string the same way the GUI does (QDate::toString()
    // with no argument == Qt::TextDate == "ddd MMM d yyyy", e.g. "Thu Apr 30 2026").
    // Falls back to the raw string if it cannot be parsed or represents an invalid date.
    auto format_date = [](const char* raw) -> std::string {
        const std::string s(raw);
        if (s.size() != 6)
        {
            return s;
        }
        int  yy = 0;
        int  mm = 0;
        int  dd = 0;
        auto r1 = std::from_chars(s.data(), s.data() + 2, yy);
        auto r2 = std::from_chars(s.data() + 2, s.data() + 4, mm);
        auto r3 = std::from_chars(s.data() + 4, s.data() + 6, dd);
        if (r1.ec != std::errc{} || r1.ptr != s.data() + 2 || r2.ec != std::errc{} || r2.ptr != s.data() + 4 || r3.ec != std::errc{} || r3.ptr != s.data() + 6)
        {
            return s;
        }
        const std::chrono::year_month_day ymd{
            std::chrono::year{2000 + yy}, std::chrono::month{static_cast<unsigned>(mm)}, std::chrono::day{static_cast<unsigned>(dd)}};
        if (!ymd.ok())
        {
            return s;
        }
        const std::chrono::weekday   wd{std::chrono::sys_days{ymd}};
        static constexpr const char* kDayNames[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static constexpr const char* kMonthNames[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::ostringstream           out;
        out << kDayNames[wd.c_encoding()] << ' ' << kMonthNames[mm] << ' ' << dd << ' ' << static_cast<int>(ymd.year());
        return out.str();
    };

    // Host system (matches the GUI's "Host System" header).
    const RdpCaptureSystemOs& os = system_info.os;
    oss << "  Host System:\n";
    oss << "    Name:                  " << os.name << '\n';
    oss << "    Description:           " << os.description << '\n';
    oss << "    Hostname:              " << os.hostname << '\n';
    if (os.memory.type[0] != '\0')
    {
        oss << "    Physical memory type:  " << os.memory.type << '\n';
    }
    oss << "    Physical memory size:  " << format_bytes_pow2(os.memory.physical, 1) << '\n';
    oss << "    Swap memory size:      " << format_bytes_pow2(os.memory.swap, 1) << '\n';
#if defined(_WIN32)
    oss << "    ETW supported:                " << (os.etw_supported ? "Yes" : "No") << '\n';
    oss << "    ETW session permission:       " << (os.etw_has_permission ? "Yes" : "No") << '\n';
    oss << "    ETW RGP registry/user group:  " << (os.etw_needs_script ? "Not configured (run AddUserToGroup.bat as Administrator)" : "Configured") << '\n';
#endif

    // Drivers (system-wide list; per-GPU mapping is shown below via driver_index).
    oss << "  Drivers (" << system_info.num_drivers << "):\n";
    for (uint64_t i = 0; i < system_info.num_drivers; ++i)
    {
        const RdpCaptureSystemDriver& drv = system_info.drivers[i];
        oss << "    [" << i << "]\n";
        if (drv.name[0] != '\0')
        {
            oss << "        Name:               " << drv.name << '\n';
        }
        if (drv.description[0] != '\0')
        {
            oss << "        Description:        " << drv.description << '\n';
        }
        if (drv.packaging_version[0] != '\0')
        {
            oss << "        Packaging version:  " << drv.packaging_version << '\n';
        }
        if (drv.packaging_date[0] != '\0')
        {
            oss << "        Packaging date:     " << format_date(drv.packaging_date) << '\n';
        }
        if (drv.software_version[0] != '\0')
        {
            oss << "        Software version:   " << drv.software_version << '\n';
        }
    }

    // CPUs.
    oss << "  CPUs (" << system_info.num_cpus << "):\n";
    for (uint64_t i = 0; i < system_info.num_cpus; ++i)
    {
        const RdpCaptureSystemCpu& cpu = system_info.cpus[i];
        oss << "    [" << i << "]\n";
        oss << "        Name:                " << cpu.name << '\n';
        oss << "        Architecture:        " << cpu.architecture << '\n';
        oss << "        Vendor ID:           " << cpu.vendor_id << '\n';
        if (cpu.cpu_id[0] != '\0')
        {
            oss << "        CPU ID:              " << cpu.cpu_id << '\n';
        }
        if (cpu.device_id[0] != '\0')
        {
            oss << "        Device ID:           " << cpu.device_id << '\n';
        }
        oss << "        Physical core count: " << cpu.num_physical_cores << '\n';
        oss << "        Logical core count:  " << cpu.num_logical_cores << '\n';
        oss << "        Speed:               " << mhz_to_ghz(cpu.max_clock_speed_mhz) << '\n';
        if (cpu.virtualization[0] != '\0')
        {
            oss << "        Virtualization:      " << cpu.virtualization << '\n';
        }
    }

    // GPUs.
    oss << "  GPUs (" << system_info.num_gpus << "):\n";
    for (uint64_t i = 0; i < system_info.num_gpus; ++i)
    {
        const RdpCaptureGpu& gpu = system_info.gpus[i];
        oss << "    [" << i << "]\n";
        oss << "        Name:                                " << gpu.name << '\n';
        const RdpCaptureSystemDriver* gpu_driver = nullptr;
        if (gpu.driver_index >= 0 && static_cast<uint64_t>(gpu.driver_index) < system_info.num_drivers)
        {
            gpu_driver = &system_info.drivers[gpu.driver_index];
        }
        else if (system_info.driver.packaging_version[0] != '\0')
        {
            // Mirror the GUI fallback (system_info_model.cpp:737-750): when this GPU
            // has no per-device driver mapping, surface the system-wide driver record.
            gpu_driver = &system_info.driver;
        }
        if (gpu_driver != nullptr && gpu_driver->packaging_version[0] != '\0')
        {
            oss << "        Driver version:                      " << gpu_driver->packaging_version << '\n';
        }
        oss << "        Shader engine clock frequency (min): " << format_hertz(gpu.asic.engine_clock_min_hz) << '\n';
        oss << "        Shader engine clock frequency (max): " << hz_to_mhz(gpu.asic.engine_clock_max_hz) << '\n';
        oss << "        Timestamp frequency:                 " << format_hertz(gpu.asic.gpu_counter_freq_hz) << '\n';
        oss << "        Family:                              " << format_hex(gpu.asic.family) << '\n';
        oss << "        Device ID:                           " << format_hex(gpu.asic.device_id) << '\n';
        oss << "        Revision:                            " << format_hex(gpu.asic.revision) << '\n';
        oss << "        eRev:                                " << format_hex(gpu.asic.e_rev) << '\n';

        oss << "        Memory:\n";
        if (gpu.memory.type[0] != '\0')
        {
            std::string memory_type = gpu.memory.type;
            std::ranges::transform(memory_type, memory_type.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            oss << "            Type:                  " << memory_type << '\n';
        }
        oss << "            Bandwidth:             " << format_bandwidth(gpu.memory.bandwidth) << '\n';
        oss << "            Bus bit width:         " << gpu.memory.bus_bit_width << '\n';
        oss << "            Clock frequency (min): " << format_hertz(gpu.memory.mem_clock_min_hz) << '\n';
        oss << "            Clock frequency (max): " << hz_to_mhz(gpu.memory.mem_clock_max_hz) << '\n';
        oss << "            Operations per clock:  " << gpu.memory.mem_ops_per_clock << '\n';
        for (uint64_t h = 0; h < gpu.memory.num_heaps; ++h)
        {
            const RdpCaptureGpuMemoryHeap& heap  = gpu.memory.heaps[h];
            const std::string              label = std::string(heap.heap_type) + " heap size:";
            oss << "            " << std::left << std::setw(23) << label << std::right << format_bytes_pow2(heap.size) << '\n';
        }

        oss << "        PCI:\n";
        oss << "            Bus:                    " << gpu.pci.bus << '\n';
        oss << "            Device:                 " << gpu.pci.device << '\n';
        oss << "            Function:               " << gpu.pci.function << '\n';
        oss << "            PCI/GPU ID:             " << gpu.pci.packed_id << '\n';
        // Settings File PCI Name: matches PciLocationToString() in
        // external/devdriver/apis/apis/dd_common_api.h ("bus%udev%ufunc%u").
        oss << "            Settings File PCI Name: bus" << gpu.pci.bus << "dev" << gpu.pci.device << "func" << gpu.pci.function << '\n';

        if (gpu.big_sw.major != 0 || gpu.big_sw.minor != 0 || gpu.big_sw.misc != 0)
        {
            oss << "        Big Software:\n";
            oss << "            Major:                 " << gpu.big_sw.major << '\n';
            oss << "            Minor:                 " << gpu.big_sw.minor << '\n';
            oss << "            Misc:                  " << gpu.big_sw.misc << '\n';
        }
    }

    {
        const std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << oss.str();
    }

    fn_table_.free_system_info(&system_info);
}

void CaptureCli::ApplyBlocklist() const
{
    fn_table_.blocklist.set_blocked_callback(context_, BlockedProcessCallback, nullptr);

    // Discard any defaults the shared API library loaded (it tries to read blocklist.ini
    // from the module directory) so the CLI's blocklist is exactly the hardcoded set
    // plus whatever the user passed on the command line.
    fn_table_.blocklist.clear(context_);

    for (const char* pattern : kCliBlocklistEntries)
    {
        if (const RdpCaptureResult result = fn_table_.blocklist.add_entry(context_, pattern); result != kRdpCaptureResultSuccess)
        {
            std::cerr << "Warning: failed to add built-in blocklist entry '" << pattern << "' (result: " << result << ")\n";
        }
    }

    for (const auto& pattern : config_.blocklist_patterns)
    {
        if (const RdpCaptureResult result = fn_table_.blocklist.add_entry(context_, pattern.c_str()); result != kRdpCaptureResultSuccess)
        {
            std::cerr << "Warning: failed to add blocklist entry '" << pattern << "' (result: " << result << ")\n";
        }
        else if (config_.verbose)
        {
            std::cout << "Blocklist: added '" << pattern << "'\n";
        }
    }

    if (!config_.blocklist_file.empty())
    {
        if (const RdpCaptureResult result = fn_table_.blocklist.load_file(context_, config_.blocklist_file.c_str()); result != kRdpCaptureResultSuccess)
        {
            std::cerr << "Warning: failed to load blocklist file '" << config_.blocklist_file << "' (result: " << result << ")\n";
        }
        else if (config_.verbose)
        {
            std::cout << "Blocklist: loaded file '" << config_.blocklist_file << "'\n";
        }
    }
}

bool ParseCommandLine(const int argc, char** argv, CaptureConfig& config)
{
    cxxopts::Options options("RadeonDeveloperPanelCLI", "RadeonDeveloperPanelCLI - GPU Capture Tool");

    // All cxxopts groups, flag names, descriptions, value types, and the
    // owning capture mode are declared once in GetCliOptionGroups(). The same
    // table drives ValidateModeFlags() so we cannot get out of sync.
    RegisterCliOptions(options);

    // Builds the interactive command reference for a given capture mode.
    // Profiling and Raytracing share the basic capture/quit commands;
    // Memory adds the snapshot command.  Returns an empty string for modes
    // that have no interactive loop (crash analysis, clocks).
    auto build_interactive_help = [](CaptureMode mode) -> std::string {
        std::ostringstream oss;
        switch (mode)
        {
        case CaptureMode::kProfiling:
            oss << " Profiling (RGP) interactive commands:\n";
            oss << "  c, capture              Trigger a capture\n";
            oss << "  mode [value[:N]]        View or set capture mode (default, frame, draw, dispatch)\n";
            oss << "                          N = render op count for draw/dispatch mode\n";
            oss << "  q, quit                 Exit the application\n";
            break;
        case CaptureMode::kMemoryTrace:
            oss << " Memory (RMV) interactive commands:\n";
            oss << "  c, capture              Dump the current memory trace to file\n";
            oss << "  s, snapshot [name]      Insert a snapshot marker (name is optional)\n";
            oss << "  q, quit                 Exit the application\n";
            break;
        case CaptureMode::kRaytracing:
            oss << " Raytracing (RRA) interactive commands:\n";
            oss << "  c, capture    Trigger a capture\n";
            oss << "  q, quit       Exit the application\n";
            break;
        default:
            break;
        }
        return oss.str();
    };

    // Sentinel values mark where interactive-command references are spliced
    // into the help output.  These sections have no cxxopts-registered flags
    // and are rendered separately.  Non-empty sentinels are used because the
    // empty string is a legitimate cxxopts group name (the anonymous group).
    static constexpr std::string_view kProfilingInteractiveSentinel  = "<<PROFILING_INTERACTIVE>>";
    static constexpr std::string_view kMemoryInteractiveSentinel     = "<<MEMORY_INTERACTIVE>>";
    static constexpr std::string_view kRaytracingInteractiveSentinel = "<<RAYTRACING_INTERACTIVE>>";

    // Map a sentinel to its capture mode, or nullopt for real cxxopts groups.
    auto sentinel_to_mode = [](const std::string& section) -> std::optional<CaptureMode> {
        if (section == kProfilingInteractiveSentinel)
            return CaptureMode::kProfiling;
        if (section == kMemoryInteractiveSentinel)
            return CaptureMode::kMemoryTrace;
        if (section == kRaytracingInteractiveSentinel)
            return CaptureMode::kRaytracing;
        return std::nullopt;
    };

    // Single source of truth for help section order.  Sentinel entries mark
    // where interactive-command references are spliced in; every other entry
    // must match a group name passed to options.add_options() above.
    const std::vector<std::string> ordered_help_sections = {
        "General",
        "Profiling (RGP)",
        std::string(kProfilingInteractiveSentinel),
        std::string(kMemoryInteractiveSentinel),
        "Raytracing (RRA)",
        std::string(kRaytracingInteractiveSentinel),
        "Crash Analysis (RGD)",
        "Clocks",
        "Blocklist",
        "System Info",
    };

    // Groups that are intentionally registered with cxxopts (so their options remain
    // parseable) but deliberately excluded from --help output. Used to keep options
    // functional without advertising them to end users.
    static const std::set<std::string> kHiddenGroupNames = {
        std::string(kHiddenRaytracingGroup),
    };

    // Drift guard: every cxxopts-registered group must either appear in
    // ordered_help_sections or be explicitly listed as a hidden group.  This catches
    // the case where a future add_options() call introduces a new group but forgets
    // to update the ordering above, which would otherwise be silently omitted from
    // --help.
    {
        std::set<std::string> ordered_set;
        for (const auto& section : ordered_help_sections)
        {
            if (!sentinel_to_mode(section).has_value())
            {
                ordered_set.insert(section);
            }
        }
        for (const auto& group : options.groups())
        {
            if (ordered_set.find(group) == ordered_set.end() && kHiddenGroupNames.find(group) == kHiddenGroupNames.end())
            {
                std::cerr << "Internal error: cxxopts group \"" << group << "\" is missing from ordered_help_sections and kHiddenGroupNames.\n";
                return false;
            }
        }
    }

    const std::string kHelpDivider = std::string(kSeparatorWidth, '-') + '\n';

    // Builds the full --help output in the desired section order with
    // interactive-command references spliced in at their sentinel positions.
    // Each section is rendered individually so that sentinels can appear
    // between any two cxxopts groups.  The only string post-processing is
    // stripping cxxopts' fixed program-description preamble (program name +
    // "[OPTION...]" + blank line) from non-first renders so the banner is not
    // duplicated.
    auto build_full_help = [&]() {
        std::string text;
        bool        is_first = true;

        for (const auto& section : ordered_help_sections)
        {
            if (const auto mode = sentinel_to_mode(section))
            {
                text += '\n';
                text += kHelpDivider;
                text += build_interactive_help(*mode);
                continue;
            }

            if (is_first)
            {
                text += options.help({section});
                is_first = false;
            }
            else
            {
                std::string  group_text   = options.help({section}, /*print_usage=*/false);
                const size_t preamble_end = group_text.find("\n\n");
                if (preamble_end != std::string::npos)
                {
                    group_text.erase(0, preamble_end + 2);
                }
                text += '\n';
                text += kHelpDivider;
                text += group_text;
            }
        }
        return text;
    };

    cxxopts::ParseResult result;
    // cxxopts uses exceptions for parse errors. CXXOPTS_NO_EXCEPTIONS would call std::exit()
    // instead, losing the ability to show help text. Keep this as the sole exception site.
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        std::cerr << "Error parsing options: " << e.what() << '\n';
        std::cout << build_full_help() << '\n';
        return false;
    }

    if (result.count("help") != 0)
    {
        if (const std::string help_mode = result["help"].as<std::string>(); !help_mode.empty())
        {
            const auto groups = GetHelpGroupsForMode(help_mode);
            if (groups.empty())
            {
                std::cerr << "Unknown mode '" << help_mode << "'. "
                          << "Valid modes: profiling, raytracing, memory, crash, clocks\n";
                return false;
            }
            std::cout << options.help(groups) << '\n';

            // Append the interactive-commands reference for modes that have one.
            std::string normalized_mode = help_mode;
            std::ranges::transform(normalized_mode, normalized_mode.begin(), ToLowerChar);
            if (normalized_mode == "profiling" || normalized_mode == "rgp")
            {
                std::cout << kHelpDivider << build_interactive_help(CaptureMode::kProfiling) << '\n';
            }
            else if (normalized_mode == "memory" || normalized_mode == "rmv")
            {
                std::cout << kHelpDivider << build_interactive_help(CaptureMode::kMemoryTrace) << '\n';
            }
            else if (normalized_mode == "raytracing" || normalized_mode == "rra")
            {
                std::cout << kHelpDivider << build_interactive_help(CaptureMode::kRaytracing) << '\n';
            }
        }
        else
        {
            std::cout << build_full_help() << '\n';
        }
        return false;
    }

    if (result.count("version") != 0)
    {
        std::cout << "RadeonDeveloperPanelCLI 1.0.0\n";
        return false;
    }

    if (result.count("output") != 0)
    {
        config.output_path = result["output"].as<std::string>();
    }

    auto mode_str = result["mode"].as<std::string>();
    std::ranges::transform(mode_str, mode_str.begin(), ToLowerChar);
    if (!ParseCaptureMode(mode_str, config))
    {
        return false;
    }

    if (!ValidateModeFlags(result, config.mode, /*mode_explicit=*/result.count("mode") > 0))
    {
        return false;
    }

    if (result.count("process") != 0)
    {
        config.process_filter = result["process"].as<std::string>();
    }

    config.verbose = result.count("verbose") > 0;

    if (!ParseRemoteConnection(result, config))
    {
        return false;
    }

    config.auto_capture_mode         = AutoCaptureMode::kNone;
    config.frame_capture_index       = 0;
    config.dispatch_start_index      = kDispatchStartIndexMinimum;
    config.dispatch_count            = 1;
    config.dispatch_capture_delay_ms = 0;

    if (result.count("rgp-auto-capture") != 0)
    {
        auto auto_capture_str = result["rgp-auto-capture"].as<std::string>();
        std::ranges::transform(auto_capture_str, auto_capture_str.begin(), ToLowerChar);
        if (!ParseAutoCapture(auto_capture_str, config))
        {
            return false;
        }
    }

    config.dispatch_capture_delay_ms = result["rgp-auto-capture-delay"].as<uint32_t>();

    auto capture_mode_str = result["rgp-capture-mode"].as<std::string>();
    std::ranges::transform(capture_mode_str, capture_mode_str.begin(), ToLowerChar);
    if (!ParseRgpCaptureMode(capture_mode_str, config))
    {
        return false;
    }

    config.rgp_render_op_count = result["rgp-render-op-count"].as<uint32_t>();
    if (config.rgp_render_op_count == 0 &&
        (config.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDraw || config.rgp_capture_mode == kRdpCaptureProfilingCaptureModeDispatch))
    {
        std::cerr << "Error: --rgp-render-op-count must be at least 1 in draw or dispatch mode\n";
        return false;
    }

    config.enable_instruction_tracing    = result.count("rgp-instruction-tracing") > 0;
    config.enable_counter_collection     = result.count("rgp-counter-collection") > 0;
    config.enable_shader_instrumentation = result.count("rgp-shader-instrumentation") > 0;

    auto sqtt_str = result["rgp-sqtt-buffer-size"].as<std::string>();
    std::ranges::transform(sqtt_str, sqtt_str.begin(), ToLowerChar);
    ParseSqttBufferSize(sqtt_str, config);

    auto rra_buf_str = result["rra-ray-history-buffer-size"].as<std::string>();
    std::ranges::transform(rra_buf_str, rra_buf_str.begin(), ToLowerChar);
    ParseRraBufferSize(rra_buf_str, config);

    config.rra_collect_ray_dispatch_data = result["rra-collect-ray-dispatch-data"].as<bool>();
    config.rra_delay_ms                  = result["rra-delay-ms"].as<uint32_t>();

    if (result.count("rra-ray-history-buffer-size") > 0 && !config.rra_collect_ray_dispatch_data)
    {
        std::cerr << "--rra-ray-history-buffer-size requires --rra-collect-ray-dispatch-data to be enabled\n";
        return false;
    }

    config.rra_enable_marker_capture = result.count("rra-marker-capture") > 0;

    if (result.count("rra-marker-begin") != 0)
    {
        config.rra_marker_begin = result["rra-marker-begin"].as<std::string>();
    }
    if (result.count("rra-marker-end") != 0)
    {
        config.rra_marker_end = result["rra-marker-end"].as<std::string>();
    }

    if (config.rra_enable_marker_capture && (config.rra_marker_begin.empty() || config.rra_marker_end.empty()))
    {
        std::cerr << "--rra-marker-capture requires both --rra-marker-begin and --rra-marker-end\n";
        return false;
    }

    config.enable_enhanced_crash_analysis = result.count("rgd-enhanced") > 0;

    config.rgd_generate_text_summary  = result.count("rgd-text-summary") > 0;
    config.rgd_generate_json_summary  = result.count("rgd-json-summary") > 0;
    config.rgd_show_marker_source     = result.count("rgd-marker-source") > 0;
    config.rgd_expand_markers         = result.count("rgd-expand-markers") > 0;
    config.rgd_pdb_include_subfolders = result.count("rgd-pdb-include-subfolders") > 0;
    if (result.count("rgd-pdb-search-path") > 0)
    {
        config.rgd_pdb_search_paths = result["rgd-pdb-search-path"].as<std::vector<std::string>>();
    }
    if (result.count("rgd-cli-path") > 0)
    {
        config.rgd_cli_path = result["rgd-cli-path"].as<std::string>();
    }
    config.rgd_collect_wave_sgprs = result.count("rgd-collect-sgprs") > 0;
    config.rgd_collect_wave_vgprs = result.count("rgd-collect-vgprs") > 0;

    if ((config.rgd_collect_wave_sgprs || config.rgd_collect_wave_vgprs) && !config.enable_enhanced_crash_analysis)
    {
        std::cerr << "--rgd-collect-sgprs/--rgd-collect-vgprs require --rgd-enhanced to take effect\n";
        return false;
    }

    config.clock_mode = kRdpCaptureGpuClocksModeUnknown;
    config.gpu_index  = result["gpu-index"].as<uint64_t>();

    if (result.count("clock-mode") != 0)
    {
        std::string clock_mode_str = result["clock-mode"].as<std::string>();
        std::ranges::transform(clock_mode_str, clock_mode_str.begin(), ToLowerChar);

        if (clock_mode_str == "normal" || clock_mode_str == "default")
        {
            config.clock_mode = kRdpCaptureGpuClocksModeNormal;
        }
        else if (clock_mode_str == "stable")
        {
            config.clock_mode = kRdpCaptureGpuClocksModeStable;
        }
        else if (clock_mode_str == "peak")
        {
            clock_mode_str = "not supported";
        }

        if (config.clock_mode == kRdpCaptureGpuClocksModeUnknown)
        {
            std::cerr << "Invalid clock mode: " << clock_mode_str << " (valid: normal, stable)" << std::endl;
            return false;
        }
    }

    config.list_blocklist = result.count("list-blocklist") > 0;
    config.system_info    = result.count("system-info") > 0;

    if (result.count("block") != 0)
    {
        config.blocklist_patterns = result["block"].as<std::vector<std::string>>();
    }

    if (result.count("block-file") != 0)
    {
        config.blocklist_file = result["block-file"].as<std::string>();
    }

    return true;
}
