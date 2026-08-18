// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a base view model helper.

#include "model/utility/userdata_view_model.h"

void UserdataViewModel::OutputPathChanged(const QString& output_path)
{
    HandleOutputPathChanged(output_path);
}
