// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the available modules widget.

#include "available_modules_widget.h"

#include <QInputDialog>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QStringListModel>
#include <QVBoxLayout>

#include <common/inc/collapsible_pane_button.h>

#include <common/inc/api/dev_tools_module.h>

#include "available_modules_delegate.h"
#include "models/module_model.h"
#include "models/preset_model.h"
#include "presets/save_preset_dialog.h"
#include "presets/std_preset_dialog.h"

namespace rdp
{
    static constexpr int kNoModulesTextBottomMargin = 12;

    AvailableModulesWidget::AvailableModulesWidget(QWidget* parent)
        : QWidget(parent)
        , proxy_model_(new AvailableModulesProxyModel())
        , list_view_(new AvailableModulesTightList(this))
        , no_modules_label_(new QLabel(this))
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        setLayout(layout);

        list_view_->setModel(proxy_model_.get());
        layout->addWidget(list_view_.get());

        connect(proxy_model_.get(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &AvailableModulesWidget::OnRowRemoved);
        connect(proxy_model_.get(), &QAbstractItemModel::rowsInserted, this, &AvailableModulesWidget::OnRowInserted);

        list_delegate_ = std::make_shared<AvailableModulesItemDelegate>();
        list_view_->setItemDelegate(list_delegate_.get());

        connect(list_delegate_.get(), &AvailableModulesItemDelegate::PressedEnable, this, &AvailableModulesWidget::EnableModule);

        no_modules_label_->setText("No modules are available");
        no_modules_label_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        no_modules_label_->setContentsMargins(0, 0, 0, kNoModulesTextBottomMargin);
        no_modules_label_->setAlignment(Qt::AlignHCenter);
        no_modules_label_->hide();
    }

    void AvailableModulesWidget::CreateButtons(std::list<QAbstractButton*>& buttons)
    {
        preset_button_ = new CollapsiblePaneButton();
        preset_button_->setIcon(QIcon(":/circle-meatballs.svg"));

        connect(preset_button_, &QPushButton::pressed, this, &AvailableModulesWidget::OpenContextMenu);

        buttons.push_back(preset_button_);
    }

    void AvailableModulesWidget::SetModuleModel(const std::shared_ptr<ModuleModel>& module_model, const std::shared_ptr<class PresetModel>& preset_model)
    {
        module_model_ = module_model;
        preset_model_ = preset_model;

        proxy_model_->setSourceModel(module_model_.get());

        connect(module_model_.get(), &ModuleModel::ModulesLocked, this, &AvailableModulesWidget::UpdateModulesLocked);
        connect(module_model_.get(), &ModuleModel::ModulesUnlocked, this, &AvailableModulesWidget::UpdateModulesLocked);
    }

    void AvailableModulesWidget::EnableModule(const QModelIndex& index)
    {
        if (module_model_ == nullptr)
        {
            return;
        }

        const QModelIndex source_model_index = proxy_model_->mapToSource(index);
        module_model_->SetModuleEnabled(module_model_->GetModuleAtIndex(source_model_index), true);
    }

    void AvailableModulesWidget::OnRowRemoved(const QModelIndex& parent, int first, int last)
    {
        if (parent.isValid())
        {
            return;
        }

        const int removed_rows = last - first + 1;
        if (proxy_model_->rowCount() - removed_rows == 0)
        {
            layout()->removeWidget(list_view_.get());
            list_view_->hide();

            layout()->addWidget(no_modules_label_.get());
            no_modules_label_->show();
        }
    }

    void AvailableModulesWidget::OnRowInserted()
    {
        if (proxy_model_->rowCount() == 1)
        {
            layout()->addWidget(list_view_.get());
            list_view_->show();

            layout()->removeWidget(no_modules_label_.get());
            no_modules_label_->hide();
        }
    }

    void AvailableModulesWidget::UpdateModulesLocked()
    {
        // When the modules are locked / unlocked we just change how they are painted and retrigger a repaint rather than adding another column or role.
        list_delegate_->SetModulesLocked(module_model_->AreModulesLocked());
        list_view_->viewport()->repaint();
    }

    AvailableModulesWidget::AvailableModulesTightList::AvailableModulesTightList(QWidget* parent)
        : TightListView(parent)
    {
        const int horizontal_margin = CollapsiblePaneStatics::GetHeaderMargin() + 1;

        setViewportMargins(horizontal_margin, 0, horizontal_margin, 0);
        setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
        setStyleSheet("QListView { background: transparent; }");
    }

    int AvailableModulesWidget::AvailableModulesTightList::GetMinHeight() const
    {
        // This widget is styled a little differently than the other tight list views and would look odd if we used the default minimum.
        return 0;
    }

    bool AvailableModulesWidget::AvailableModulesProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
    {
        if (source_parent.isValid())
        {
            return false;
        }

        const auto* model = static_cast<const ModuleModel*>(sourceModel());

        // System modules are hosted in the SYSTEM tab and should not appear in the available features list.
        const QModelIndex     source_index = model->index(source_row, 0, {});
        const DevToolsModule* module       = model->GetModuleAtIndex(source_index);
        if (module != nullptr && module->GetModuleCategory() == ModuleCategory::kSystem)
        {
            return false;
        }

        const QAbstractItemModel* source = sourceModel();
        return !source->data(source->index(source_row, ModuleModel::ModuleModelColumns::kModuleModelColumnsEnabled, {})).toBool();
    }

    void AvailableModulesWidget::OpenContextMenu()
    {
        if (preset_model_ == nullptr)
        {
            return;
        }

        QMenu    menu;
        QAction* load_action   = menu.addAction("Load preset");
        QAction* save_action   = menu.addAction("Save preset");
        QAction* delete_action = menu.addAction("Delete preset");

        const bool are_modules_unlocked = !module_model_->AreModulesLocked();
        load_action->setEnabled(are_modules_unlocked);
        save_action->setEnabled(are_modules_unlocked);
        delete_action->setEnabled(are_modules_unlocked && !preset_model_->GetCustomPresets().isEmpty());

        menu.addSeparator();

        for (const QString& builtin_preset : preset_model_->GetBuiltInPresets())
        {
            QAction* new_action = menu.addAction(QString("Load %1").arg(builtin_preset));
            new_action->setProperty("preset", builtin_preset);
            new_action->setEnabled(are_modules_unlocked);
        }

        menu.addSeparator();

        for (const QString& custom_preset : preset_model_->GetRecentCustomPresets())
        {
            QAction* new_action = menu.addAction(QString("Load %1").arg(custom_preset));
            new_action->setProperty("preset", custom_preset);
            new_action->setEnabled(are_modules_unlocked);
        }

        const QSize button_size = preset_button_->geometry().size();
        QAction*    action      = menu.exec(preset_button_->mapToGlobal(QPoint{button_size.width() / 2, button_size.height() / 2}));

        if (action == nullptr)
        {
            return;
        }

        if (action == load_action)
        {
            LoadPreset();
            return;
        }

        if (action == save_action)
        {
            SavePreset();
            return;
        }

        if (action == delete_action)
        {
            DeletePreset();
            return;
        }

        preset_model_->LoadPreset(action->property("preset").toString());
    }

    void AvailableModulesWidget::LoadPreset()
    {
        if (preset_model_ == nullptr)
        {
            return;
        }

        StdPresetDialog dialog(preset_model_->GetPresets(), "Load preset", [&](const auto& preset_name) { preset_model_->LoadPreset(preset_name); });
        dialog.setWindowTitle("Load preset");

        dialog.exec();
    }

    void AvailableModulesWidget::SavePreset()
    {
        if (preset_model_ == nullptr)
        {
            return;
        }

        SavePresetDialog dialog(preset_model_);
        dialog.setWindowTitle("Save preset");

        dialog.exec();
    }

    void AvailableModulesWidget::DeletePreset()
    {
        if (preset_model_ == nullptr)
        {
            return;
        }

        StdPresetDialog dialog(preset_model_->GetCustomPresets(), "Delete preset", [&](const auto& preset_name) { preset_model_->DeletePreset(preset_name); });
        dialog.setWindowTitle("Delete preset");

        dialog.exec();
    }
}  // namespace rdp
