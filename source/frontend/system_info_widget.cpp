// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP system information widget implementation.

#include "system_info_widget.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QMessageBox>

#include "ui_system_info_widget.h"

/// @brief The title for the choose folder dialog for the system info dump.
static constexpr char const* kBrowseFolderTitle = "Choose dump output directory";

/// @brief The description in the message box if the dump was successful.
static constexpr char const* kDumpSuccessful = "System info successfully written to disk.";

namespace rdp
{
    SystemInfoWidget::SystemInfoWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::SystemInfoWidget)
        , system_info_container_(nullptr)
    {
        ui_->setupUi(this);
        ui_->export_button->setEnabled(false);

        system_info_container_layout_ = new QVBoxLayout;
        ui_->scrollAreaWidgetContents->setLayout(system_info_container_layout_);

        connect(ui_->export_button, &QPushButton::pressed, this, &SystemInfoWidget::DumpInfoPressed);
        ModelValidityChanged(false);
    }

    SystemInfoWidget::~SystemInfoWidget() = default;

    void SystemInfoWidget::OnSystemInfoModelLoaded(std::shared_ptr<SystemInfoModel> model)
    {
        system_info_model_ = std::move(model);

        if (system_info_container_ != nullptr)
        {
            system_info_container_layout_->removeWidget(system_info_container_);
            delete system_info_container_;
            system_info_container_ = nullptr;
        }

        system_info_container_ = new QWidget;
        system_info_container_->setContentsMargins(0, 0, 0, 0);

        const auto& form_layout = system_info_model_->GetFormLayout();
        system_info_container_->setLayout(form_layout);
        system_info_container_layout_->addWidget(system_info_container_);
        system_info_container_layout_->addStretch();

        // Use a unique connection since the system model can load more than once
        connect(system_info_model_.get(), &SystemInfoModel::IsDataValidChanged, this, &SystemInfoWidget::ModelValidityChanged, Qt::UniqueConnection);
    }

    void SystemInfoWidget::DumpInfoPressed()
    {
        const QString& new_output_path = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
            this, kBrowseFolderTitle, QString(), QFileDialog::Option::ShowDirsOnly | QFileDialog::Option::DontResolveSymlinks));

        if (new_output_path.isEmpty())
        {
            return;
        }

        const bool result = system_info_model_->ExportInfo(new_output_path);
        if (result)
        {
            QMessageBox message_box;
            message_box.setText(kDumpSuccessful);
            message_box.exec();
        }
    }
    void SystemInfoWidget::ModelValidityChanged(bool is_valid)
    {
        ui_->stacked_widget->setCurrentIndex(is_valid ? 0 : 1);
        ui_->export_button->setEnabled(is_valid);
    }

}  // namespace rdp
