// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the save preset dialog.

#include "save_preset_dialog.h"

#include <QComboBox>

#include "models/preset_model.h"

#include "ui_save_preset_dialog.h"

namespace rdp
{
    SavePresetDialog::SavePresetDialog(const std::shared_ptr<PresetModel>& preset_model, QWidget* widget)
        : QDialog(widget)
        , ui_(new Ui::SavePresetDialog)
        , preset_model_(preset_model)
    {
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setFocusPolicy(Qt::StrongFocus);

        ui_->setupUi(this);

        QPalette overwrite_palette = ui_->overwrite_warning->palette();
        overwrite_palette.setColor(QPalette::WindowText, Qt::darkYellow);
        ui_->overwrite_warning->setPalette(overwrite_palette);

        connect(ui_->preset_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SavePresetDialog::PresetComboBoxChanged);
        connect(ui_->preset_name_line_edit, &QLineEdit::textChanged, this, &SavePresetDialog::CustomPresetNameChanged);

        if (preset_model_ != nullptr)
        {
            ui_->preset_combo_box->addItems(preset_model_->GetPresets());
        }

        ui_->preset_combo_box->addItem("Create new...");

        connect(ui_->cancel_button, &QPushButton::pressed, this, &SavePresetDialog::Cancel);
        connect(ui_->ok_button, &QPushButton::pressed, this, &SavePresetDialog::Accept);

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    SavePresetDialog::~SavePresetDialog() = default;

    void SavePresetDialog::showEvent(QShowEvent* event)
    {
        QDialog::showEvent(event);
        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    void SavePresetDialog::PresetComboBoxChanged(int index)
    {
        if (index == ui_->preset_combo_box->count() - 1)
        {
            ui_->preset_name_input->show();
            ui_->overwrite_warning->hide();
        }
        else
        {
            ui_->preset_name_input->hide();
            ui_->overwrite_warning->show();

            const QString preset_name = ui_->preset_combo_box->itemText(index);
            ui_->overwrite_warning->setText(QString("The settings in %1 will be overwritten").arg(preset_name));
        }

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    void SavePresetDialog::CustomPresetNameChanged(const QString& text)
    {
        if (preset_model_ == nullptr)
        {
            return;
        }

        ui_->overwrite_warning->setVisible(preset_model_->GetPresets().contains(text));
        ui_->overwrite_warning->setText(QString("The settings in %1 will be overwritten").arg(text));

        setFixedHeight(QDialog::minimumSizeHint().height());
    }

    void SavePresetDialog::Cancel()
    {
        close();
    }

    void SavePresetDialog::Accept()
    {
        if (preset_model_ == nullptr)
        {
            close();
            return;
        }

        const int     current_index = ui_->preset_combo_box->currentIndex();
        const QString preset_name =
            current_index == ui_->preset_combo_box->count() - 1 ? ui_->preset_name_line_edit->text() : ui_->preset_combo_box->currentText();

        if (!preset_model_->CreateOrUpdatePreset(preset_name))
        {
            return;
        }

        close();
    }
};  // namespace rdp
