// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Raytracing module definitions file

#ifndef RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_MODULE_DEFINITIONS_H_
#define RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_MODULE_DEFINITIONS_H_

static const char*           kOutputPathKey                = "output_path";
static const char*           kFileConceptName              = "Scene";
static const char*           kRraFileExtension             = "rra";
static constexpr const char* kRayHistoryBufferSizeIndexKey = "ray_history_buffer_size_index";
static constexpr const char* kRayHistoryBufferCustomKey    = "ray_history_buffer_size_custom";
static const constexpr int   kCaptureKeyId                 = 2;

/// Delay between the application's driver finishing device init and the automatic trace request made when marker
/// capture is enabled. The driver registers its ray tracing trace sources shortly after device init, and a trace
/// requested before that has no acceleration structure or ray history data. 500 ms is an estimate that should be safe on any device.
static constexpr uint32_t kMarkerCaptureArmDelayMs = 500;

using RayHistoryBufferSizeIndex = devtrace::RayHistoryBufferIndex;

#endif
