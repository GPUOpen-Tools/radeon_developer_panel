// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility view V2 class implementation,

#include "utility_view_v2.h"

#include <QTabWidget>
#include <QVBoxLayout>

#include <MercuryModuleExt.h>

#include <common/inc/definitions.h>

#include "ui_utility_view.h"

UtilityViewV2::UtilityViewV2(QWidget* parent)
    : QWidget(parent)
    , base_ui_(new Ui::UtilityView)
{
    base_ui_->setupUi(this);
}

UtilityViewV2::~UtilityViewV2() noexcept = default;

void UtilityViewV2::SetBaseModel(const std::shared_ptr<UserdataViewModel>& view_model)
{
    base_view_model_ = view_model;

    base_view_model_->OnBind();
}

void UtilityViewV2::SetContents(QWidget* contents)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    Q_ASSERT(layout);

    layout->insertWidget(0, contents);
}
