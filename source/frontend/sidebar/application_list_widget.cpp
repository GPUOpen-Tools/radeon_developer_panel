// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the application list widget.

#include "application_list_widget.h"

#include <QGridLayout>
#include <QInputDialog>
#include <QWindow>

#include <qt_common/custom_widgets/message_overlay.h>

#include <common/inc/collapsible_pane_button.h>
#include <common/inc/rdp_combo_box.h>
#include <common/inc/show_selection_no_focus_delegate.h>

#include "models/api_model.h"
#include "models/api_proxy_model.h"
#include "models/application_model.h"
#include "models/application_proxy_model.h"
#include "models/sidebar/auto_connect_model.h"

#include "add_application_dialog.h"
#include "application_context_menu.h"

#include "ui_executable_list_widget.h"

namespace rdp
{
    ApplicationListWidget::ApplicationListWidget(QWidget* parent)
        : ExecutableListWidget(parent)
        , auto_connect_model_(std::make_shared<AutoConnectModel>())
        , desc_label_(new QLabel(this))
        , auto_connect_label_(new QLabel("Auto connect:", this))
        , api_label_(new QLabel("API:", this))
        , auto_connect_combo_box_(new RdpComboBox(this))
        , api_combo_box_(new RdpComboBox(this))
    {
        SetTextDescription("Applications will appear here once they are added to the panel.");

        // Create a grid layout for the filter combo boxes
        auto* filter_grid_layout = new QGridLayout();
        filter_grid_layout->setSpacing(5);
        filter_grid_layout->setContentsMargins(ui_->verticalLayout->spacing(), 0, ui_->verticalLayout->spacing(), 0);

        // Set up labels
        auto_connect_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        api_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        // Set up combo boxes
        auto_connect_combo_box_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto_connect_combo_box_->setModel(auto_connect_model_.get());

        api_combo_box_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

        // Add widgets to grid layout
        filter_grid_layout->addWidget(auto_connect_label_, 0, 0);
        filter_grid_layout->addWidget(auto_connect_combo_box_, 0, 1);
        filter_grid_layout->addWidget(api_label_, 1, 0);
        filter_grid_layout->addWidget(api_combo_box_, 1, 1);

        // Insert the filter layout at the top of the vertical layout
        ui_->verticalLayout->insertLayout(0, filter_grid_layout);

        ui_->verticalLayout->insertWidget(1, desc_label_);
        desc_label_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        desc_label_->setStyleSheet("font-size: 7pt");
        desc_label_->setContentsMargins(ui_->verticalLayout->spacing(), 0, ui_->verticalLayout->spacing(), 0);
        desc_label_->setMinimumWidth(300);
        desc_label_->setWordWrap(true);

        ui_->list_view->setItemDelegate(new ShowSelectionNoFocusDelegate(this));

        QPalette palette = ui_->list_view->palette();
        palette.setColor(QPalette::Inactive, QPalette::Highlight, palette.color(QPalette::Active, QPalette::Highlight));
        palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::HighlightedText));
        ui_->list_view->setPalette(palette);

        ui_->list_view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui_->list_view, &QListView::customContextMenuRequested, this, &ApplicationListWidget::ContextMenuRequested);
    }

    void ApplicationListWidget::CreateButtons(std::list<QAbstractButton*>& buttons)
    {
        const auto add_button = new CollapsiblePaneButton();
        add_button->setIcon(QIcon(":/circle-plus-enabled.svg"));

        connect(add_button, &QPushButton::pressed, this, &ApplicationListWidget::AddApplication);

        buttons.push_back(add_button);
    }

    void ApplicationListWidget::SetModels(const std::shared_ptr<ApplicationModel>& application_model,
                                          const std::shared_ptr<BlocklistModel>&   blocklist_model,
                                          const std::shared_ptr<ConnectionModel>&  connection_model,
                                          const std::shared_ptr<ApiProxyModel>&    api_proxy_model)
    {
        application_model_ = application_model;
        blocklist_model_   = blocklist_model;
        connection_model_  = connection_model;
        api_proxy_model_   = api_proxy_model;

        // Set up the embedded combo boxes
        api_combo_box_->setModel(api_proxy_model_.get());
        connect(api_combo_box_, QOverload<int>::of(&QComboBox::activated), this, &ApplicationListWidget::HandleApiFilterChosen);
        connect(auto_connect_combo_box_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ApplicationListWidget::HandleAutoConnectComboBoxChanged);

        // Connect to model signals for combo boxes
        connect(application_model_.get(), &ApplicationModel::AutoConnectModeChanged, this, &ApplicationListWidget::OnAutoConnectionModeChanged);
        connect(application_model_.get(), &ApplicationModel::ApiFilterChanged, this, &ApplicationListWidget::OnApiFilterChanged);
        connect(api_proxy_model_.get(), &ApiProxyModel::SourceApiCountChanged, this, &ApplicationListWidget::SourceApiCountChanged);

        proxy_model_ = std::make_unique<ApplicationProxyModel>();
        proxy_model_->SetSourceApplicationModel(application_model_);

        // This is workaround to the table view not repainting when an application is launched since RDP will not be in focus.
        // We also have to register the metatype because for whatever reason Qt doesn't do that itself despite declaring a signal
        // that uses it.
        qRegisterMetaType<QVector<int>>();
        connect(application_model_.get(), &ApplicationModel::ApplicationConnected, this, &ApplicationListWidget::OnApplicationConnected);
        connect(application_model_.get(), &QAbstractItemModel::dataChanged, this, &ApplicationListWidget::ForceRepaint);
        connect(
            application_model_.get(), &ApplicationModel::EditedAppNameButAlreadyOnBlocklist, this, &ApplicationListWidget::EditedAppNameButAlreadyOnBlocklist);

        connect(proxy_model_.get(), &QAbstractItemModel::rowsInserted, this, &ApplicationListWidget::RowsInserted);
        connect(proxy_model_.get(), &QAbstractItemModel::rowsRemoved, this, &ApplicationListWidget::RowNumberChanged);
        connect(proxy_model_.get(), &QAbstractItemModel::modelReset, this, &ApplicationListWidget::RowNumberChanged);
        RowNumberChanged();

        SetBaseModel(proxy_model_.get());
        SelectionChanged({});

        connect(ui_->list_view->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ApplicationListWidget::SelectionChanged);
        connect(proxy_model_.get(), &QAbstractItemModel::dataChanged, this, &ApplicationListWidget::DataChanged);

        if (proxy_model_->rowCount() > 0)
        {
            ui_->list_view->selectionModel()->setCurrentIndex(proxy_model_->index(0, 0), QItemSelectionModel::ClearAndSelect);
        }

        connect(application_model_.get(), &ApplicationModel::ConnectionBehaviorChanged, this, [this](const QString& description) {
            desc_label_->setText(description);
            updateGeometry();
        });
    }

    void ApplicationListWidget::RowsInserted()
    {
        if (proxy_model_->rowCount() == 1 && ui_->list_view->selectionModel() != nullptr)
        {
            ui_->list_view->selectionModel()->setCurrentIndex(proxy_model_->index(0, 0), QItemSelectionModel::ClearAndSelect);
        }

        RowNumberChanged();
    }

    void ApplicationListWidget::RowNumberChanged()
    {
        SetShowingTextDescription(proxy_model_->rowCount() == 0);
    }

    void ApplicationListWidget::OnApplicationConnected(const QModelIndex& index) const
    {
        if (proxy_model_ == nullptr)
        {
            return;
        }

        const QModelIndex proxy_index = proxy_model_->mapFromSource(index);
        ui_->list_view->setCurrentIndex(proxy_index);
    }

    void ApplicationListWidget::ForceRepaint() const
    {
        ui_->list_view->viewport()->repaint();
    }

    void ApplicationListWidget::AddApplication()
    {
        if (application_model_ == nullptr)
        {
            return;
        }

        AddApplicationDialog dialog(blocklist_model_);
        dialog.setWindowTitle("Add application");

        connect(&dialog, &AddApplicationDialog::AddApplication, [&](const QString& app_name) { application_model_->AddApplication(app_name); });

        dialog.exec();
    }

    void ApplicationListWidget::SelectionChanged(const QModelIndex& index) const
    {
        if (application_model_ == nullptr || proxy_model_ == nullptr)
        {
            return;
        }

        const QModelIndex source_index = proxy_model_->mapToSource(index);
        application_model_->SetModuleDisplayApplication(source_index);
    }

    void ApplicationListWidget::DataChanged() const
    {
        SelectionChanged(ui_->list_view->selectionModel()->currentIndex());
    }

    void ApplicationListWidget::ContextMenuRequested(const QPoint& pos) const
    {
        if (application_model_ == nullptr || blocklist_model_ == nullptr)
        {
            return;
        }

        const QModelIndex index = ui_->list_view->indexAt(pos);
        if (!index.isValid())
        {
            return;
        }

        const bool             is_connected = connection_model_ != nullptr && connection_model_->IsConnected();
        ApplicationContextMenu menu(proxy_model_->mapToSource(index), application_model_, blocklist_model_, is_connected);
        menu.exec(ui_->list_view->mapToGlobal(pos));
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void ApplicationListWidget::EditedAppNameButAlreadyOnBlocklist(const QString& app_name)
    {
        MessageOverlay::CriticalAsync("Cannot edit application name",
                                      QString("The application name couldn't be changed to %1 because that application is on the blocklist.").arg(app_name));
    }

    void ApplicationListWidget::HandleApiFilterChosen(const int index) const
    {
        if (application_model_ == nullptr || api_proxy_model_ == nullptr)
        {
            return;
        }

        const auto api = static_cast<ApiModel::Api>(api_proxy_model_->data(api_proxy_model_->index(index, 0), Qt::UserRole).toInt());
        application_model_->SetApiFilter(api);
    }

    void ApplicationListWidget::HandleAutoConnectComboBoxChanged(const int index) const
    {
        if (application_model_ == nullptr)
        {
            return;
        }

        const uint raw_mode = auto_connect_model_->data(auto_connect_model_->index(index, 0, {}), Qt::UserRole).toUInt();
        application_model_->SetAutoConnectionMode(static_cast<ApplicationAutoConnectMode>(raw_mode));
    }

    void ApplicationListWidget::OnApiFilterChanged(const ApiModel::Api new_filter) const
    {
        if (application_model_ == nullptr || api_proxy_model_ == nullptr)
        {
            return;
        }

        for (int row = 0; row < api_proxy_model_->rowCount(); ++row)
        {
            if (const QModelIndex row_index = api_proxy_model_->index(row, 0);
                api_proxy_model_->data(row_index, Qt::UserRole).toInt() == static_cast<int>(new_filter))
            {
                api_combo_box_->setCurrentIndex(row);
                break;
            }
        }
    }

    void ApplicationListWidget::OnAutoConnectionModeChanged(const ApplicationAutoConnectMode mode) const
    {
        auto_connect_combo_box_->setCurrentIndex(mode);
    }

    void ApplicationListWidget::SourceApiCountChanged(const int count) const
    {
        api_combo_box_->setEnabled(count != 0);
    }

}  // namespace rdp
