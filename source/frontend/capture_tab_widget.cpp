// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the widget that shows all of the module tabs with the client views.

#include "capture_tab_widget.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QModelIndex>
#include <QStyle>
#include <QStyleOptionTabWidgetFrame>
#include <QTabBar>

#include <common/inc/api/dev_tools_module.h>

#include <qt_common/custom_widgets/message_overlay.h>
#include <qt_common/utils/qt_util.h>

#include "models/module_model.h"
#include "models/preset_model.h"

namespace rdp
{
    CaptureTabWidget::CaptureTabWidget(QWidget* parent)
        : QTabWidget(parent)
    {
        connect(this, &QTabWidget::tabCloseRequested, this, &CaptureTabWidget::CloseTab);
        connect(&QtCommon::QtUtils::ColorTheme::Get(), &QtCommon::QtUtils::ColorTheme::ColorThemeUpdated, this, &CaptureTabWidget::OnColorThemeChanged);
    }

    CaptureTabWidget::~CaptureTabWidget()
    {
        // Remove all tabs first so the internal QStackedWidget no longer tracks
        // these widgets.  Without this, deleting a view triggers a current-widget
        // adjustment inside QStackedWidget that can access an already-deleted
        // sibling view, causing a crash.
        while (count() > 0)
        {
            removeTab(0);
        }

        for (const auto& pair : module_views_)
        {
            delete pair.second;
        }

        module_views_.clear();
    }

    int CaptureTabWidget::GetMinimumHeightOffset() const
    {
        QStyleOptionTabWidgetFrame opt;
        initStyleOption(&opt);
        opt.state = QStyle::State_None;

        return style()->sizeFromContents(QStyle::CT_TabWidget, &opt, {}, this).height() + 1;
    }

    void CaptureTabWidget::SetModels(const std::shared_ptr<class ModuleModel>& module_model, const std::shared_ptr<class PresetModel>& preset_model)
    {
        module_model_ = module_model;
        preset_model_ = preset_model;

        connect(module_model_.get(), &ModuleModel::ModuleStatusChanged, this, &CaptureTabWidget::OnModuleStatusChanged);
        connect(module_model_.get(), &ModuleModel::ModuleFailedToChangeStatus, this, &CaptureTabWidget::OnModuleFailedToChangeStatus);

        connect(module_model_.get(), &ModuleModel::ModulesLocked, this, &CaptureTabWidget::OnModulesLocked);
        connect(module_model_.get(), &ModuleModel::ModulesUnlocked, this, &CaptureTabWidget::OnModulesUnlocked);

        connect(preset_model_.get(), &PresetModel::PresetLoaded, this, &CaptureTabWidget::OnPresetLoaded);

        AddNoModulesTab();

        for (const DevToolsModule* module : module_model_->GetModules())
        {
            // System modules are hosted in the SYSTEM tab, not the CAPTURE tab.
            if (module->GetModuleCategory() == ModuleCategory::kSystem)
            {
                continue;
            }

            module_views_.insert(std::make_pair(module, module->CreateView()));

            if (module->IsEnabled())
            {
                AddModuleView(module);
            }
        }

        OnPresetLoaded();
    }

    void CaptureTabWidget::ApplyNoModulesStyle()
    {
        // We use a blank svg that is the same size as the xmark so that the tab size won't change if it is bounded by the close button.
        // Similarly, we use a transparent "A" title for the tab in case the tab size is bounded by the text height.
        setStyleSheet("QTabBar::tab { color: transparent; background: transparent; } QTabWidget::pane { border-top: none } " +
                      GetCloseButtonStyleSheet("xmark-blank"));
    }

    void CaptureTabWidget::AddNoModulesTab()
    {
        ApplyNoModulesStyle();

        QLabel* label = new QLabel(this);
        label->setAlignment(Qt::AlignCenter);
        label->setText(
            "No features are enabled. Features can be enabled by loading a preset or\nby pressing the enable button in the available features pane.");

        QFont label_font = label->font();
        label_font.setPointSize(8);
        label->setFont(label_font);

        addTab(label, "A");
    }

    void CaptureTabWidget::OnColorThemeChanged()
    {
        ColorThemeType current_theme = QtCommon::QtUtils::ColorTheme::Get().GetColorTheme();
        if (current_theme == kColorThemeTypeCount)
        {
            current_theme = QtCommon::QtUtils::DetectOsSetting();
        }

        if (current_theme == ColorThemeType::kColorThemeTypeLight)
        {
            xmark_ = "xmark";
        }
        else
        {
            xmark_ = "xmark-dm";
        }

        if (total_module_view_tabs_ == 0)
        {
            ApplyNoModulesStyle();
        }
        else
        {
            setStyleSheet(GetCloseButtonStyleSheet(xmark_));
        }
    }

    void CaptureTabWidget::RemoveNoModulesTab()
    {
        setStyleSheet(GetCloseButtonStyleSheet(xmark_));

        QWidget* no_modules_tab = widget(0);
        removeTab(0);

        delete no_modules_tab;
    }

    QString CaptureTabWidget::GetCloseButtonStyleSheet(const QString& icon_name)
    {
        return QString("QTabBar::close-button {image: url(:/%1.svg); subcontrol-position: left; }").arg(icon_name);
    }

    void CaptureTabWidget::OnModuleStatusChanged(const DevToolsModule* module, bool is_enabled)
    {
        // System modules are hosted in the SYSTEM tab, not the CAPTURE tab.
        if (module->GetModuleCategory() == ModuleCategory::kSystem)
        {
            return;
        }

        if (is_enabled)
        {
            AddModuleView(module);
            return;
        }

        RemoveModuleView(module);
    }

    void CaptureTabWidget::OnModuleFailedToChangeStatus(const DevToolsModule* module, bool is_enabled)
    {
        const QString verb        = is_enabled ? "Enabled" : "Disabled";
        const QString module_name = module->GetModuleDisplayName().c_str();

        const QString title = QString("Feature Couldn't be %1").arg(verb);
        const QString text  = QString("%1 couldn't be %2 because an unexpected error occurred.").arg(module_name, verb.toLower());
        const QString key   = QString("%1-%2-error").arg(module_name, verb);

        MessageOverlay::CriticalAsync(title, text, key);
    }

    void CaptureTabWidget::AddModuleView(const DevToolsModule* module_to_add)
    {
        if (module_model_ == nullptr || module_views_.count(module_to_add) != 1)
        {
            return;
        }

        // We want to ensure that we don't add the tab twice. In most cases this call will just be a no-op.
        RemoveModuleView(module_to_add);

        if (total_module_view_tabs_ == 0)
        {
            RemoveNoModulesTab();
        }

        // Tabs should always display in the same order as contained in module_model_->GetModules(), regardless of the insertion order.
        int insertion_index = 0;
        for (const DevToolsModule* module : module_model_->GetModules())
        {
            // System modules are hosted in the SYSTEM tab, not the CAPTURE tab.
            if (module->GetModuleCategory() == ModuleCategory::kSystem)
            {
                continue;
            }

            if (module_to_add == module)
            {
                insertTab(insertion_index, module_views_[module], module->GetModuleDisplayName().c_str());

                // We want to switch to the current tab as the user is manually adding modules.
                // During preset loading, we'll likely end up on the last tab since we don't differentiate between
                // a module changing status during a preset and the user adding one. So we subscribe to the PresetLoaded()
                // signal and switch to the first tab.
                setCurrentIndex(insertion_index);

                break;
            }

            // The tab for this module was in the tab view, so we need to insert after it
            if (GetTabIndexForModule(module) != -1)
            {
                ++insertion_index;
            }
        }

        ++total_module_view_tabs_;
    }

    void CaptureTabWidget::RemoveModuleView(const DevToolsModule* module)
    {
        const int tab_index = GetTabIndexForModule(module);
        if (tab_index == -1)
        {
            return;
        }

        removeTab(tab_index);

        if (--total_module_view_tabs_ == 0)
        {
            AddNoModulesTab();
        }
    }

    int CaptureTabWidget::GetTabIndexForModule(const DevToolsModule* module)
    {
        if (module_views_.count(module) != 1)
        {
            return -1;
        }

        QWidget* module_view = module_views_[module];
        for (int tab = 0; tab < count(); ++tab)
        {
            if (widget(tab) == module_view)
            {
                return tab;
            }
        }

        return -1;
    }

    void CaptureTabWidget::CloseTab(int index)
    {
        if (module_model_ == nullptr)
        {
            return;
        }

        for (const auto& pair : module_views_)
        {
            if (pair.second == widget(index))
            {
                module_model_->SetModuleEnabled(pair.first, false);
                return;
            }
        }
    }

    void CaptureTabWidget::OnModulesLocked()
    {
        setTabsClosable(false);
    }

    void CaptureTabWidget::OnModulesUnlocked()
    {
        setTabsClosable(true);
    }

    void CaptureTabWidget::OnPresetLoaded()
    {
        setCurrentIndex(0);
    }

}  // namespace rdp
