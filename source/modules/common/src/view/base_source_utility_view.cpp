// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Base utility view class implementation.

#include "common/inc/view/base_utility_view.h"

#include <QStandardPaths>

#include <source_userdata.h>

BaseUtilityView::BaseUtilityView()
{
}

BaseUtilityView::~BaseUtilityView() = default;

void BaseUtilityView::SetBaseModel(const std::shared_ptr<UserdataViewModel>& view_model)
{
    UtilityViewV2::SetBaseModel(view_model);
}
