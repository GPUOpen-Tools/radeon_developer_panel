// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP profiling module view class implementation.

#include "profiling_view.h"

#include "ui_profiling_view.h"

#include <qt_common/custom_widgets/message_overlay.h>
#include <ui_profiling_capture.h>

#include <common/inc/rdf_file_dialog.h>

#include "common/inc/definitions.h"
#include "profiling_module_definitions.h"
#include "profiling_userdata_view_model.h"

#include "common/inc/collapsible_pane.h"
#include "common/inc/file_client_view.h"
#include "profiling_userdata_view.h"

constexpr const char* kExeMissingTitle   = "Radeon GPU Profiler missing";
constexpr const char* kExeMissingMessage = "Radeon GPU Profiler executable not found. Please specify path in settings.";

ProfilingView::ProfilingView(ProfilingUserdataView*                             userdata_view,
                             const std::shared_ptr<ProfilingViewModel>&         view_model,
                             const std::shared_ptr<ProfilingUserdataViewModel>& userdata_view_model,
                             QWidget*                                           parent)
    : SplitClientFileView("Profiling", "Profile", "rgp", parent)
    , ui_(new Ui::ProfilingView)
    , view_model_(view_model)
    , userdata_view_model_(userdata_view_model)
    , userdata_view_(userdata_view)
{
    current_connection_model_      = new CurrentConnectionModel(this);
    available_capture_modes_model_ = new QStringListModel(this);

    auto* capture_widget = new CollapsiblePane<QWidget>(this);
    capture_widget->SetTitleText("Capture");

    ui_->setupUi(capture_widget->GetBody());
    AddContentWidget(capture_widget);
    AddContentWidget(userdata_view);
    AddSpecializedCaptureOptions();
    SetupConnections();

    ui_->collect_data_button->setText("Capture profile");

    SetConnectedProcessText("");

    // Hide render op count row by default; it will be shown when Draw or Dispatch mode is selected
    ui_->render_op_count_row->setVisible(false);

    if (getenv(kEnableRdfInspectorEnv) != nullptr)
    {
        file_client_view_->AddCustomDropdownOption(
            "Inspect file",
            [](const QString&) -> bool { return true; },
            [&](const QString& path) {
                const auto dialog = new RdfFileDialog("RGP File Chunks", path, this);
                dialog->show();
            });
        file_client_view_->AddCustomDropdownOption(
            "Inspect files",
            [](const QString&) -> bool { return true; },
            [&](const QString& path) {
                const auto dialog = new RdfFileDialog("RGP File Chunks", path, this);
                dialog->show();
            },
            true);
    }
}

ProfilingView::~ProfilingView() = default;

void ProfilingView::AddSpecializedCaptureOptions() const
{
    auto* control_layout = ui_->main_layout;
    Q_ASSERT(control_layout != nullptr);

    const auto widget = new QWidget;
    userdata_view_->GetCaptureUi()->setupUi(widget);
    control_layout->addWidget(widget);

    // Hide internal-only options in public builds
    userdata_view_->GetCaptureUi()->disable_capture_timeout->hide();
    userdata_view_->GetCaptureUi()->enable_legacy_capture->hide();

    if (getenv("RDP_RGP_ALLOW_SINGLE_TOKEN_SQTT") == nullptr)
    {
        userdata_view_->GetCaptureUi()->enable_single_token_sqtt->hide();
    }
}

void ProfilingView::SetupConnections()
{
    model_binder_.StartBinding();

    model_binder_.Connect(ui_->capture_mode_selection, &QComboBox::activated, this, &ProfilingView::OnCaptureModeSelectionChanged);
    model_binder_.Connect(ui_->active_connection_selection, &QComboBox::activated, this, &ProfilingView::OnCaptureTargetSelectionChanged);

    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    model_binder_.Connect(ui_->collect_data_button, &QPushButton::clicked, view_model.get(), &ProfilingViewModel::RequestBeginTrace);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ConnectedProcessTextChanged, this, &SplitClientView::SetConnectedProcessText);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ClientStatusChanged, this, &SplitClientView::SetStatus);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::CurrentConnectionsChanged, this, &ProfilingView::OnCurrentConnectionsChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::UiStatusChanged, this, &ProfilingView::OnUiStatusChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ProgressInfoStep, ui_->capture_progress_widget, &CaptureProgressWidget::UpdateProgress);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::ExecutableMissing, this, &ProfilingView::ApplicationExecutableMissing);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::TraceComplete, this, &ProfilingView::OnTraceEnded);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::TraceFailed, this, &ProfilingView::OnTraceFailed);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::TraceAborted, this, &ProfilingView::OnTraceAborted);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::AvailableCaptureModesChanged, this, &ProfilingView::OnAvailableCaptureModesChanged);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::ShowCaptureUi, this, &ProfilingView::OnEnableCaptureUi);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::ShowProgressUi, this, &ProfilingView::OnEnableProgressUi);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::OutputPathInfoChanged, this, &SplitClientFileView::OnOutputPathChanged);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::CurrentCaptureModeChanged, this, &ProfilingView::OnCurrentCaptureModeChanged);
    model_binder_.Connect(view_model.get(), &ProfilingViewModel::InvalidTraceCounterData, this, &ProfilingView::OnInvalidTraceCounterData);

    connect(ui_->render_op_count_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProfilingView::OnRenderOpCountBoxChanged);

    model_binder_.Connect(file_client_view_.get(), &FileClientView::OpenFile, view_model.get(), &TraceSourceViewModel::OnOpenFile);
    model_binder_.Connect(file_client_view_.get(), &FileClientView::RemovedFile, view_model.get(), &TraceSourceViewModel::OnRemovedFile);

    model_binder_.Connect(ui_->capture_progress_widget,
                          &CaptureProgressWidget::TraceCancelled,
                          view_model.get(),
                          static_cast<void (ProfilingViewModel::*)()>(&ProfilingViewModel::RequestAbort));

    model_binder_.Connect(ui_->hotkey_edit, &GlobalShortcutEdit::ShortcutTriggered, view_model.get(), &ProfilingViewModel::RequestBeginTrace);

    model_binder_.Connect(ui_->capture_mode_default_button, &QPushButton::clicked, this, &ProfilingView::OnSetDefaultCaptureModeClicked);

    const auto userdata_model = userdata_view_model_.lock();
    Q_ASSERT(userdata_model != nullptr);

    model_binder_.Connect(
        ui_->hotkey_edit, &GlobalShortcutEdit::ShortcutChanged, userdata_model.get(), &ProfilingUserdataViewModel::HandleCaptureShortcutChanged);

    model_binder_.Connect(userdata_model.get(), &ProfilingUserdataViewModel::DrawCountChanged, this, &ProfilingView::OnDrawCountChanged);
    model_binder_.Connect(userdata_model.get(), &ProfilingUserdataViewModel::DispatchCountChanged, this, &ProfilingView::OnDispatchCountChanged);

    model_binder_.Connect(
        userdata_model.get(), &ProfilingUserdataViewModel::CaptureDelayChanged, ui_->capture_delay_widget, &CaptureDelayWidget::OnCaptureDelayChanged);
    model_binder_.Connect(userdata_model.get(),
                          &ProfilingUserdataViewModel::ShouldDelayCaptureChanged,
                          ui_->capture_delay_widget,
                          &CaptureDelayWidget::OnShouldDelayCaptureChanged);
    model_binder_.Connect(ui_->capture_delay_widget,
                          &CaptureDelayWidget::ShouldDelayCaptureChanged,
                          userdata_model.get(),
                          &ProfilingUserdataViewModel::HandleShouldDelayCaptureChanged);
    model_binder_.Connect(
        ui_->capture_delay_widget, &CaptureDelayWidget::CaptureDelayChanged, userdata_model.get(), &ProfilingUserdataViewModel::HandleCaptureDelayChanged);

    model_binder_.Connect(userdata_model.get(), &ProfilingUserdataViewModel::CaptureShortcutChanged, this, &ProfilingView::OnUserDataShortcutChanged);

    // Forward TraceSupportChanged from ProfilingViewModel to ProfilingUserdataViewModel
    model_binder_.Connect(
        view_model.get(), &ProfilingViewModel::TraceSupportChanged, userdata_model.get(), &ProfilingUserdataViewModel::HandleTraceSupportChanged);

    // Forward CurrentConnectionsChanged to determine prelaunch settings editable state
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::CurrentConnectionsChanged, this, &ProfilingView::OnPrelaunchSettingsEditableStateChanged);

    // Also update prelaunch settings when the connected process text changes (handles disconnect after kDone state)
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ConnectedProcessTextChanged, this, &ProfilingView::OnConnectedProcessTextChanged);

    // Connect to auto capture settings editable signal to enable/disable hotkey and delay widgets
    model_binder_.Connect(
        userdata_model.get(), &ProfilingUserdataViewModel::AutoCaptureSettingsEditableChanged, this, &ProfilingView::OnAutoCaptureSettingsEditableChanged);

    ui_->hotkey_edit->SetDefaultShortcut(userdata_model->GetDefaultShortcut());

#ifdef Q_OS_LINUX
    // Global hotkey capture is not supported on Linux (no X11 dependency).
    // Hide the hotkey UI elements.
    ui_->hotkey_label->hide();
    ui_->hotkey_edit->hide();
#endif

    ui_->active_connection_selection->setModel(current_connection_model_);
    ui_->capture_mode_selection->setModel(available_capture_modes_model_);

    // Set initial state - combo box should be disabled when there are no connections
    ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());

    // Request UI sync with model
    view_model->Update();
}

void ProfilingView::OnInvalidTraceCounterData(const QString& path, const RgpFileValidatorStatus status)
{
    QString message;
    switch (status)
    {
    case kRgpFileValidatorStatusSpmError:
        message = "Profile file does not contain valid SPM counter data. Delete the profile?";
        break;
    case kRgpFileValidatorStatusMissingChunkError:
        message = "Profile file is missing one or more required chunks. Delete the profile?";
        break;
    default:
        message = "Profile file is invalid. Delete the profile?";
        break;
    }

    MessageOverlay::QuestionAsync("Invalid profile detected", message, "", [=](const QDialogButtonBox::StandardButton result) {
        if (result == QDialogButtonBox::Yes)
        {
            if (QFile file(path); file.exists())
            {
                file.remove();
            }
        }
    });
}

void ProfilingView::OnCurrentConnectionsChanged(std::unordered_map<DDConnectionId, devtrace::Api> connections)
{
    current_connection_model_->OnCurrentConnectionsChanged(connections);
    ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());

    // Sync the combo box selection with the current capture target from the view model
    const auto view_model = view_model_.lock();
    if (view_model != nullptr)
    {
        const uint16_t current_target = view_model->GetCaptureTarget();
        for (int i = 0; i < current_connection_model_->rowCount(QModelIndex()); ++i)
        {
            if (current_connection_model_->GetConnectionIdForIndex(i) == current_target)
            {
                ui_->active_connection_selection->setCurrentIndex(i);
                break;
            }
        }
    }
}

void ProfilingView::OnCaptureTargetSelectionChanged(int index)
{
    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const uint16_t connection_id = current_connection_model_->GetConnectionIdForIndex(index);
    view_model->SetCaptureTarget(connection_id);
}

void ProfilingView::OnCaptureModeSelectionChanged(int index)
{
    const auto model = view_model_.lock();
    Q_ASSERT(index >= 0 && std::cmp_less(index, model->GetAvailableCaptureModes().size()));
    const uint32_t mode = model->GetAvailableCaptureModes().at(index).mode;
    model->SetCaptureMode(mode);
    OnCurrentCaptureModeChanged(mode);
}

void ProfilingView::ApplicationExecutableMissing([[maybe_unused]] const QString& path)
{
    MessageOverlay::CriticalAsync(kExeMissingTitle, kExeMissingMessage);
}

void ProfilingView::OnUserDataShortcutChanged(const GlobalShortcut& shortcut)
{
    ui_->hotkey_edit->SetShortcut(shortcut);
}

void ProfilingView::OnCurrentCaptureModeChanged(uint32_t capture_mode)
{
    const auto available_modes = view_model_.lock()->GetAvailableCaptureModes();
    for (int index = 0; index < static_cast<int>(available_modes.size()); ++index)
    {
        if (available_modes[index].mode == capture_mode)
        {
            ui_->capture_mode_selection->setCurrentIndex(index);
            break;
        }
    }

    const auto rgp_mode     = static_cast<devtrace::RgpCaptureMode>(capture_mode);
    const bool is_render_op = (rgp_mode == devtrace::RgpCaptureMode::kDraw || rgp_mode == devtrace::RgpCaptureMode::kDispatch);
    ui_->render_op_count_row->setVisible(is_render_op);

    if (is_render_op)
    {
        const auto userdata_model = userdata_view_model_.lock();
        if (userdata_model != nullptr)
        {
            // Sync the spinbox to the appropriate count value for the current mode
            const QSignalBlocker blocker(ui_->render_op_count_spinbox);
            if (rgp_mode == devtrace::RgpCaptureMode::kDraw)
            {
                ui_->render_op_count_label->setText("Draw count:");
            }
            else
            {
                ui_->render_op_count_label->setText("Dispatch count:");
            }
        }
    }
}

void ProfilingView::OnTraceFailed()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Error(GetModuleName().toStdString() + " capture failed.", 0, 0);
    }

    OnTraceEnded();
}

void ProfilingView::OnTraceAborted()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Warning(GetModuleName().toStdString() + " capture was aborted.", 0, 0);
    }

    OnTraceEnded();
}

void ProfilingView::OnTraceEnded()
{
    ReloadFiles();

    OnEnableCaptureUi();
}

void ProfilingView::OnAvailableCaptureModesChanged(const std::vector<CaptureMode>& modes)
{
    Q_ASSERT(!modes.empty());

    QStringList items;
    for (const CaptureMode& mode : modes)
    {
        items.push_back(mode.text);
    }

    available_capture_modes_model_->setStringList(items);

    // Only enable capture mode selection if there are multiple modes AND the UI is currently enabled
    const bool ui_enabled     = ui_->collect_data_button->isEnabled();
    const bool multiple_modes = items.size() > 1;
    ui_->capture_mode_selection->setEnabled(ui_enabled && multiple_modes);

    for (int index = 0; index < static_cast<int>(modes.size()); ++index)
    {
        available_capture_modes_model_->setData(available_capture_modes_model_->index(index), modes[index].mode, Qt::UserRole);
    }
}

void ProfilingView::OnUiStatusChanged(bool should_enable)
{
    ui_->collect_data_button->setEnabled(should_enable);
    ui_->capture_mode_default_button->setEnabled(should_enable);

    // Also update capture mode dropdown - should be enabled only when UI is enabled and there are multiple modes
    const bool multiple_modes = available_capture_modes_model_->rowCount() > 1;
    ui_->capture_mode_selection->setEnabled(should_enable && multiple_modes);
}

void ProfilingView::OnEnableCaptureUi()
{
    ui_->capture_widget_stack->setCurrentWidget(ui_->collect_data_page);
    ui_->capture_progress_widget->Reset();
}

void ProfilingView::OnEnableProgressUi()
{
    ui_->capture_widget_stack->setCurrentWidget(ui_->progress_page);
    // TODO: Support can or cannot cancel
    //ui_->capture_progress_widget->SetCancelButtonShown(info.can_cancel);
}

void ProfilingView::OnSetDefaultCaptureModeClicked()
{
    const auto view_model = view_model_.lock();
    if (view_model != nullptr)
    {
        view_model->UpdateDefaultCaptureMode();
    }
}

void ProfilingView::OnPrelaunchSettingsEditableStateChanged([[maybe_unused]] std::unordered_map<DDConnectionId, devtrace::Api> connections)
{
    // Prelaunch settings are editable when no application is connected (i.e., pid == 0)
    // We need to check the actual status pid, not just the connections map
    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const auto& status   = view_model->GetSourceStatus();
    const bool  editable = (status.pid == 0);

    const auto userdata_model = userdata_view_model_.lock();
    if (userdata_model != nullptr)
    {
        userdata_model->HandlePrelaunchSettingsEditableChanged(editable);
    }
}

void ProfilingView::OnConnectedProcessTextChanged([[maybe_unused]] const QString& connected_process_text)
{
    // When the connected process text changes (especially becomes empty on disconnect),
    // we need to update the prelaunch settings editable state.
    // This handles the case where a client disconnects after being in kDone state,
    // where CurrentConnectionsChanged may not fire because the connections map was already empty.
    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const auto& status   = view_model->GetSourceStatus();
    const bool  editable = (status.pid == 0);

    const auto userdata_model = userdata_view_model_.lock();
    if (userdata_model != nullptr)
    {
        userdata_model->HandlePrelaunchSettingsEditableChanged(editable);
    }
}

void ProfilingView::OnAutoCaptureSettingsEditableChanged(bool enabled)
{
    // Enable/disable hotkey and delay widgets when auto capture settings editable state changes
    ui_->hotkey_edit->setEnabled(enabled);
    ui_->capture_delay_widget->setEnabled(enabled);
}

void ProfilingView::OnRenderOpCountBoxChanged(int count)
{
    const auto userdata_model = userdata_view_model_.lock();
    if (userdata_model == nullptr)
    {
        return;
    }

    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const auto rgp_mode = static_cast<devtrace::RgpCaptureMode>(view_model->GetCaptureMode());
    if (rgp_mode == devtrace::RgpCaptureMode::kDraw)
    {
        userdata_model->HandleDrawCountChanged(static_cast<uint32_t>(count));
    }
    else if (rgp_mode == devtrace::RgpCaptureMode::kDispatch)
    {
        userdata_model->HandleDispatchCountChanged(static_cast<uint32_t>(count));
    }
}

void ProfilingView::OnDrawCountChanged(uint32_t draw_count)
{
    const auto view_model = view_model_.lock();
    if (view_model && static_cast<devtrace::RgpCaptureMode>(view_model->GetCaptureMode()) == devtrace::RgpCaptureMode::kDraw)
    {
        const QSignalBlocker blocker(ui_->render_op_count_spinbox);
        ui_->render_op_count_spinbox->setValue(static_cast<int>(draw_count));
    }
}

void ProfilingView::OnDispatchCountChanged(uint32_t dispatch_count)
{
    const auto view_model = view_model_.lock();
    if (view_model && static_cast<devtrace::RgpCaptureMode>(view_model->GetCaptureMode()) == devtrace::RgpCaptureMode::kDispatch)
    {
        const QSignalBlocker blocker(ui_->render_op_count_spinbox);
        ui_->render_op_count_spinbox->setValue(static_cast<int>(dispatch_count));
    }
}
