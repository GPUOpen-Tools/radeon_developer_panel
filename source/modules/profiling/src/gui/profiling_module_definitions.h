// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module definitions file

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_MODULE_DEFINITIONS_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_MODULE_DEFINITIONS_H_

#include <array>
#include <cstdint>

static const constexpr char* kNodeKey                   = "ProfilingUserData";
static const constexpr char* kOutputPathKey             = "output_path";
static const constexpr char* kComputeAutoTriggerKey     = "opencl_auto_trigger";
static const constexpr char* kDispatchCountKey          = "dispatch_count";
static constexpr const char* kComputeAutoCaptureTimeKey = "compute_auto_capture_time_ms";
static const constexpr char* kSqttBufferProfileKey      = "sqtt_buffer_profile_index";
static const constexpr char* kFrameTriggerKey           = "frame_trigger";

static constexpr const char* kTraceProgressSpmCounter = "Processing counter data...";

static const char* kSpmCounterPathKey = "spm_counter_path";

static const std::array<uint32_t, 12> kSpmSampleFrequencies      = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65535};
static const int                      kDefaultSpmSampleFrequency = 4096;

static const constexpr char* kSQTTCustomBufferSizeKey = "sqtt_buffer_custom_size";

static const int kDefaultDispatchStart     = 1;
static const int kDefaultDispatchEnd       = 10;
static const int kDefaultAutoCaptureTime   = 100;
static const int kDefaultFrameCaptureIndex = 5;

static const int kCaptureKeyId = 1;  ///< Capture shortcut identifier used with GlobalShortcutManager

#endif
