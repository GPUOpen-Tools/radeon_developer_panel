// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Memory trace module utility view model class implementation.

#include "memory_trace_userdata_view_model.h"

MemoryTraceUserdataViewModel::MemoryTraceUserdataViewModel(std::shared_ptr<devtrace::RmvUserdataMapper>&  mapper,
                                                           const std::string&                             output_path_parent_folder,
                                                           const std::function<void(const std::string&)>& apply_fn)
    : BaseUserdataViewModel(mapper, std::make_shared<AlwaysPrelaunchSettingsHelper>(), output_path_parent_folder, apply_fn)
{
}

bool MemoryTraceUserdataViewModel::ReceiveUserData([[maybe_unused]] const std::string& data)
{
    return true;
}
