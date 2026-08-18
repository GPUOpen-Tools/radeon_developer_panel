// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Base module utility view class definition.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_VIEW_BASE_UTILITY_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_VIEW_BASE_UTILITY_VIEW_H_

#include <memory>

#include <QObject>

#include <MercuryModuleExt.h>
#include <ddModule.h>

#include "common/inc/model/utility/userdata_view_model.h"
#include "common/inc/utility_view_v2.h"

class BaseUtilityView : public UtilityViewV2
{
    Q_OBJECT
public:
    /// @brief Constructor.
    explicit BaseUtilityView();

    /// @brief Destructor.
    ~BaseUtilityView() override;

    /// @brief Sets the model for this view.
    /// @param [in] view_model The new model to use with this view.
    void SetBaseModel(const std::shared_ptr<UserdataViewModel>& view_model) override;
};

#endif
