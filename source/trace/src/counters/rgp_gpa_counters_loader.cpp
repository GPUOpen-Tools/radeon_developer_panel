// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools
/// @file
/// @brief  Static member definitions for GPUPerfAPICountersEntryPoints.

#define GPA_COUNTERS_LIB_KEEP_LOADED
#include <spm_db/gpa_counters_loader.h>

GPUPerfAPICountersEntryPoints* GPUPerfAPICountersEntryPoints::instance_ = nullptr;
std::once_flag                 GPUPerfAPICountersEntryPoints::init_instance_flag_;
