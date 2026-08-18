// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for dialog that sets the custom DevDriver timeouts.

#include "timeout_dialog.h"

#include "models/timeout_model.h"

#include "ui_timeout_dialog.h"

namespace rdp
{
    TimeoutDialog::TimeoutDialog(const std::shared_ptr<class TimeoutModel>& timeout_model)
        : timeout_model_(timeout_model)
        , ui_(new Ui::TimeoutDialog())
    {
        ui_->setupUi(this);

        setFixedHeight(minimumSizeHint().height());

        // This only needs to be done once since saving the timeouts requires restarting the panel
        CustomDDTimeouts timeouts = timeout_model_->GetSavedTimeouts();
        ui_->retry_spin_box->setValue(timeouts.retry_timeout_ms);
        ui_->communication_spin_box->setValue(timeouts.communication_timeout_ms);
        ui_->connection_timeout_spin_box->setValue(timeouts.connection_timeout_ms);

        connect(ui_->save_button, &QPushButton::pressed, this, &TimeoutDialog::SaveTimeouts);
        connect(ui_->reset_button, &QPushButton::pressed, this, &TimeoutDialog::ResetTimeouts);
    }

    TimeoutDialog::~TimeoutDialog()
    {
    }

    void TimeoutDialog::SaveTimeouts()
    {
        CustomDDTimeouts timeouts{};
        timeouts.retry_timeout_ms         = static_cast<long>(ui_->retry_spin_box->value());
        timeouts.communication_timeout_ms = static_cast<long>(ui_->communication_spin_box->value());
        timeouts.connection_timeout_ms    = static_cast<long>(ui_->connection_timeout_spin_box->value());

        timeout_model_->SetTimeouts(timeouts);
    }

    void TimeoutDialog::ResetTimeouts()
    {
        timeout_model_->ResetTimeouts();
    }
}  // namespace rdp
