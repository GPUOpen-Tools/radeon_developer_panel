// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the standard preset dialog.

#include "std_preset_dialog.h"

#include <QComboBox>

#include "models/preset_model.h"

#include "ui_std_preset_dialog.h"

namespace rdp
{
    StdPresetDialog::StdPresetDialog(const QStringList& options, const QString& action_text, const std::function<void(const QString&)>& action, QWidget* widget)
        : QDialog(widget)
        , ui_(new Ui::StdPresetDialog)
        , action_(action)
    {
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setFocusPolicy(Qt::StrongFocus);

        ui_->setupUi(this);
        ui_->title_label->setText(action_text + ":");
        ui_->preset_combo_box->addItems(options);

        connect(ui_->cancel_button, &QPushButton::pressed, this, &StdPresetDialog::Cancel);
        connect(ui_->ok_button, &QPushButton::pressed, this, &StdPresetDialog::Accept);

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    StdPresetDialog::~StdPresetDialog() = default;

    void StdPresetDialog::showEvent(QShowEvent* event)
    {
        QDialog::showEvent(event);
        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    void StdPresetDialog::Cancel()
    {
        close();
    }

    void StdPresetDialog::Accept()
    {
        const QString preset_name = ui_->preset_combo_box->currentText();
        if (action_)
        {
            action_(preset_name);
        }

        close();
    }
};  // namespace rdp
