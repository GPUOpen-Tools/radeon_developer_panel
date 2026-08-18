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

using RayHistoryBufferSizeIndex = devtrace::RayHistoryBufferIndex;

#endif
