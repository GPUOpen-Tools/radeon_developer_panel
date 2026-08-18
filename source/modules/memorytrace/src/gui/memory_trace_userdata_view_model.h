// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Memory trace module utility view model class definition.

#ifndef RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_UTILITY_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_UTILITY_VIEW_MODEL_H_

#include <memory>

#include <source_userdata.h>
#include <source_userdata_mapper.h>

#include <common/inc/model/utility/base_userdata_view_model.h>

/// @brief View model for the memory trace utility view.
class MemoryTraceUserdataViewModel final : public BaseUserdataViewModel<devtrace::RmvUserdataMapper>
{
public:
    /// @brief Constructor.
    /// @param [in] mapper Object used to map userdata to and from JSON.
    /// @param [in] output_path_parent_folder The name of the default output path in the user's documents folder.
    /// @param [in] apply_fn The function used to apply changes.
    MemoryTraceUserdataViewModel(std::shared_ptr<devtrace::RmvUserdataMapper>&  mapper,
                                 const std::string&                             output_path_parent_folder,
                                 const std::function<void(const std::string&)>& apply_fn);

    bool ReceiveUserData(const std::string& data) override;
};

#endif
