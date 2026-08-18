// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP implementation for the add application dialog.

#include "add_application_dialog.h"

#include "models/blocklist_model.h"

#include "ui_add_application_dialog.h"

namespace rdp
{
    AddApplicationDialog::AddApplicationDialog(const std::shared_ptr<BlocklistModel>& blocklist_model, QWidget* widget)
        : QDialog(widget)
        , ui_(new Ui::AddApplicationDialog)
        , blocklist_model_(blocklist_model)
    {
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setFocusPolicy(Qt::StrongFocus);

        ui_->setupUi(this);

        ui_->error_message->hide();

        connect(ui_->application_line_edit, &QLineEdit::textChanged, this, &AddApplicationDialog::AppTextChanged);
        connect(ui_->cancel_button, &QPushButton::pressed, this, &AddApplicationDialog::Cancel);
        connect(ui_->ok_button, &QPushButton::pressed, this, &AddApplicationDialog::Accept);

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    AddApplicationDialog::~AddApplicationDialog() = default;

    void AddApplicationDialog::AppTextChanged(const QString& text)
    {
        if (blocklist_model_ == nullptr)
        {
            return;
        }

        const bool blocklist_contains_name = blocklist_model_->Contains(text);
        ui_->ok_button->setEnabled(!blocklist_contains_name);
        ui_->error_message->setVisible(blocklist_contains_name);

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    void AddApplicationDialog::Cancel()
    {
        close();
    }

    void AddApplicationDialog::Accept()
    {
        emit AddApplication(ui_->application_line_edit->text());
        close();
    }
};  // namespace rdp
