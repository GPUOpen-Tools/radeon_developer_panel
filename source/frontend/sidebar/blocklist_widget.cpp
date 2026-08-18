// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the blocklist widget.

#include "blocklist_widget.h"

#include <QInputDialog>

#include <common/inc/collapsible_pane_button.h>

#include "add_application_dialog.h"
#include "models/blocklist_model.h"

#include "ui_executable_list_widget.h"

namespace rdp
{
    BlocklistListWidget::BlocklistProxyModel::BlocklistProxyModel(QObject* parent)
        : QSortFilterProxyModel(parent)
    {
        QSortFilterProxyModel::sort(0);
    }

    bool BlocklistListWidget::BlocklistProxyModel::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
    {
        return source_left.data().toString().toLower() < source_right.data().toString().toLower();
    }

    BlocklistListWidget::BlocklistListWidget(QWidget* parent)
        : ExecutableListWidget(parent)
        , proxy_model_(new BlocklistProxyModel(this))
    {
        SetTextDescription("Blocked applications will appear here once the panel is connected.");

        // Setup context menu
        context_menu_.addAction("Edit Item", this, &BlocklistListWidget::OnEditItem);
        context_menu_.addAction("Remove Item", this, &BlocklistListWidget::OnRemoveItem);
        ui_->list_view->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(ui_->list_view, &QListView::customContextMenuRequested, this, &BlocklistListWidget::ContextMenuRequested);
    }

    void BlocklistListWidget::CreateButtons(std::list<QAbstractButton*>& buttons)
    {
        QIcon add_icon = QIcon(":/circle-plus-enabled.svg");
        add_icon.addFile(":/circle-plus-disabled.svg", {}, QIcon::Mode::Disabled);

        add_button_ = new CollapsiblePaneButton();
        add_button_->setIcon(add_icon);

        QIcon refresh_icon = QIcon(":/circle-refresh-enabled.svg");
        refresh_icon.addFile(":/circle-refresh-disabled.svg", {}, QIcon::Mode::Disabled);

        reset_button_ = new CollapsiblePaneButton();
        reset_button_->setIcon(refresh_icon);

        connect(add_button_, &QPushButton::pressed, this, &BlocklistListWidget::AddApplication);

        buttons.push_back(add_button_);
        buttons.push_back(reset_button_);
    }

    void BlocklistListWidget::SetModel(const std::shared_ptr<BlocklistModel>& model)
    {
        model_ = model;

        proxy_model_->setSourceModel(model_.get());
        SetBaseModel(proxy_model_.get());

        connect(model_.get(), &BlocklistModel::PlatformChanged, this, &BlocklistListWidget::OnPlatformChanged);
        connect(reset_button_, &QPushButton::pressed, model_.get(), &BlocklistModel::RestoreDefaults);

        model_->OnBind();
    }

    void BlocklistListWidget::OnPlatformChanged(BlocklistModel::Platform platform)
    {
        const bool is_platform_unknown = platform == BlocklistModel::Platform::kUnknown;
        SetShowingTextDescription(is_platform_unknown);

        add_button_->setDisabled(is_platform_unknown);
        reset_button_->setDisabled(is_platform_unknown);

        QString platform_string;
        switch (platform)
        {
        case BlocklistModel::Platform::kLinux:
            platform_string = "Linux";
            break;
        case BlocklistModel::Platform::kWindows:
            platform_string = "Windows";
            break;
        default:
            platform_string = "";
            break;
        }

        emit PlatformNameChanged(platform_string);
    }

    void BlocklistListWidget::AddApplication()
    {
        if (model_ == nullptr)
        {
            return;
        }

        AddApplicationDialog dialog;
        dialog.setWindowTitle("Add application to blocklist");

        connect(&dialog, &AddApplicationDialog::AddApplication, [&](const QString& app_name) { model_->AddApplication(app_name); });

        dialog.exec();
    }

    void BlocklistListWidget::ContextMenuRequested(const QPoint& pos)
    {
        if (model_ == nullptr)
        {
            return;
        }

        const QModelIndex index = ui_->list_view->indexAt(pos);
        if (!index.isValid())
        {
            return;
        }

        context_menu_.popup(ui_->list_view->mapToGlobal(pos));
    }

    void BlocklistListWidget::OnEditItem()
    {
        if (model_ == nullptr)
        {
            return;
        }

        ui_->list_view->edit(ui_->list_view->currentIndex());
    }

    void BlocklistListWidget::OnRemoveItem()
    {
        if (proxy_model_ == nullptr || model_ == nullptr)
        {
            return;
        }

        proxy_model_->removeRow(ui_->list_view->currentIndex().row());
    }
}  // namespace rdp
