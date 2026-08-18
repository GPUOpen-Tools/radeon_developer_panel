// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing view implementation.

#include "raytracing_view.h"
#include "ui_raytracing_capture.h"
#include "ui_raytracing_view.h"

#include <QHBoxLayout>

#include <qt_common/custom_widgets/message_overlay.h>

#include <common/inc/rdf_file_dialog.h>
#include "common/inc/collapsible_pane.h"
#include "common/inc/definitions.h"
#include "common/inc/file_client_view.h"

constexpr auto kExeMissingTitle   = "Radeon Raytracing Analyzer missing";
constexpr auto kExeMissingMessage = "Radeon Raytracing Analyzer executable not found. Please specify path in settings.";

RaytracingView::RaytracingView(RaytracingUserdataView*                             userdata_view,
                               const std::shared_ptr<RaytracingViewModel>&         view_model,
                               const std::shared_ptr<RaytracingUserdataViewModel>& userdata_view_model,
                               QWidget*                                            parent)
    : SplitClientFileView("Raytracing", "Scene", "rra", parent)
    , ui_(new Ui::RaytracingView)
    , view_model_(view_model)
    , userdata_view_model_(userdata_view_model)
    , userdata_view_(userdata_view)
{
    current_connection_model_ = new CurrentConnectionModel(this);

    auto* capture_widget = new CollapsiblePane<QWidget>(this);
    capture_widget->SetTitleText("Capture");

    ui_->setupUi(capture_widget->GetBody());
    AddContentWidget(capture_widget);
    AddContentWidget(userdata_view_);
    AddSpecializedCaptureOptions();
    SetupConnections();

    SetConnectedProcessText("");

    if (getenv(kEnableRdfInspectorEnv) != nullptr)
    {
        file_client_view_->AddCustomDropdownOption(
            "Inspect file",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                auto* dialog = new RdfFileDialog("RRA File Chunks", path, this);
                dialog->show();
            });
        file_client_view_->AddCustomDropdownOption(
            "Inspect files",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                auto* dialog = new RdfFileDialog("RRA File Chunks", path, this);
                dialog->show();
            },
            true);
    }
}

void RaytracingView::AddSpecializedCaptureOptions() const
{
    auto* control_layout = ui_->main_layout;
    Q_ASSERT(control_layout != nullptr);

    const auto widget = new QWidget;
    userdata_view_->GetCaptureUi()->setupUi(widget);
    control_layout->addWidget(widget);
}

void RaytracingView::SetupConnections()
{
    model_binder_.StartBinding();

    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    model_binder_.Connect(ui_->collect_data_button, &QPushButton::clicked, view_model.get(), &RaytracingViewModel::RequestBeginTrace);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::CaptureMissingBvhData, this, &RaytracingView::OnCaptureMissingBvhData);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::CaptureMissingRayHistory, this, &RaytracingView::OnCaptureMissingRayDispatchData);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::CaptureIncompleteRayHistory, this, &RaytracingView::OnCaptureIncompleteRayDispatchData);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ConnectedProcessTextChanged, this, &SplitClientView::SetConnectedProcessText);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ClientStatusChanged, this, &SplitClientView::SetStatus);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::CurrentConnectionsChanged, this, &RaytracingView::OnCurrentConnectionsChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::UiStatusChanged, this, &RaytracingView::OnUiStatusChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ProgressInfoStep, ui_->capture_progress_widget, &CaptureProgressWidget::UpdateProgress);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::ExecutableMissing, this, &RaytracingView::ApplicationExecutableMissing);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::TraceComplete, this, &RaytracingView::OnTraceEnded);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::TraceFailed, this, &RaytracingView::OnTraceFailed);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::TraceAborted, this, &RaytracingView::OnTraceAborted);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::ShowCaptureUi, this, &RaytracingView::OnEnableCaptureUi);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::ShowProgressUi, this, &RaytracingView::OnEnableProgressUi);
    model_binder_.Connect(view_model.get(), &RaytracingViewModel::OutputPathInfoChanged, this, &SplitClientFileView::OnOutputPathChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::DisabledReasonChanged, this, &SplitClientView::UpdateUnsupportedDescription);

    model_binder_.Connect(file_client_view_.get(), &FileClientView::OpenFile, view_model.get(), &TraceSourceViewModel::OnOpenFile);
    model_binder_.Connect(file_client_view_.get(), &FileClientView::RemovedFile, view_model.get(), &TraceSourceViewModel::OnRemovedFile);

    model_binder_.Connect(ui_->capture_progress_widget,
                          &CaptureProgressWidget::TraceCancelled,
                          view_model.get(),
                          static_cast<void (RaytracingViewModel::*)()>(&RaytracingViewModel::RequestAbort));

    model_binder_.Connect(ui_->hotkey_edit, &GlobalShortcutEdit::ShortcutTriggered, view_model.get(), &RaytracingViewModel::RequestBeginTrace);

    const auto userdata_model = userdata_view_model_.lock();
    Q_ASSERT(userdata_model != nullptr);

    model_binder_.Connect(
        ui_->hotkey_edit, &GlobalShortcutEdit::ShortcutChanged, userdata_model.get(), &RaytracingUserdataViewModel::HandleCaptureShortcutChanged);

    model_binder_.Connect(userdata_model.get(), &RaytracingUserdataViewModel::CaptureShortcutChanged, this, &RaytracingView::OnUserDataShortcutChanged);

    // Forward TraceSupportChanged from RaytracingViewModel to RaytracingUserdataViewModel
    model_binder_.Connect(
        view_model.get(), &RaytracingViewModel::TraceSupportChanged, userdata_model.get(), &RaytracingUserdataViewModel::HandleTraceSupportChanged);

    model_binder_.Connect(
        userdata_model.get(), &RaytracingUserdataViewModel::CaptureDelayChanged, ui_->capture_delay_widget, &CaptureDelayWidget::OnCaptureDelayChanged);
    model_binder_.Connect(userdata_model.get(),
                          &RaytracingUserdataViewModel::ShouldDelayCaptureChanged,
                          ui_->capture_delay_widget,
                          &CaptureDelayWidget::OnShouldDelayCaptureChanged);
    model_binder_.Connect(ui_->capture_delay_widget,
                          &CaptureDelayWidget::ShouldDelayCaptureChanged,
                          userdata_model.get(),
                          &RaytracingUserdataViewModel::HandleShouldDelayCaptureChanged);
    model_binder_.Connect(
        ui_->capture_delay_widget, &CaptureDelayWidget::CaptureDelayChanged, userdata_model.get(), &RaytracingUserdataViewModel::HandleCaptureDelayChanged);

    ui_->hotkey_edit->SetDefaultShortcut(userdata_model->GetDefaultShortcut());
    ui_->active_connection_selection->setModel(current_connection_model_);

#ifdef Q_OS_LINUX
    // Global hotkey capture is not supported on Linux (no X11 dependency).
    // Hide the hotkey UI elements.
    ui_->hotkey_label->hide();
    ui_->hotkey_edit->hide();
#endif

    // Set initial state - combo box should be disabled when there are no connections
    ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());

    // Request UI sync with model
    view_model->Update();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RaytracingView::OnCaptureMissingBvhData()
{
    MessageOverlay::CriticalAsync("No acceleration structures captured",
                                  "There are no acceleration structures captured in the trace file. Please ensure that the application being traced has ray "
                                  "tracing enabled and ray tracing is taking place at the time of capture. Also ensure that top-level"
                                  " acceleration structures are built in each frame and not "
                                  "destroyed, cleared, or reused in that same frame. Otherwise, a few more capture attempts may be necessary to get a "
                                  "full capture in some instances.");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RaytracingView::OnCaptureMissingRayDispatchData()
{
    MessageOverlay::CriticalAsync("No ray dispatch data captured",
                                  "There are no ray dispatch chunks present in the trace file. Please ensure that all command buffers are rebuilt each frame.");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RaytracingView::OnCaptureIncompleteRayDispatchData()
{
    MessageOverlay::WarningAsync("Incomplete dispatch capture",
                                 "The selected buffer size is too low for this capture. "
                                 "Please increase the buffer size or try capturing with a smaller resolution.");
}

void RaytracingView::OnUserDataShortcutChanged(const GlobalShortcut& shortcut) const
{
    ui_->hotkey_edit->SetShortcut(shortcut);
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void RaytracingView::OnEnableCaptureUi() const
{
    ui_->capture_widget_stack->setCurrentWidget(ui_->collect_data_page);
    ui_->capture_progress_widget->Reset();
}

void RaytracingView::OnEnableProgressUi() const
{
    ui_->capture_widget_stack->setCurrentWidget(ui_->progress_page);
}

void RaytracingView::OnCurrentConnectionsChanged(const std::unordered_map<DDConnectionId, devtrace::Api>& connections) const
{
    current_connection_model_->OnCurrentConnectionsChanged(connections);
    ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RaytracingView::ApplicationExecutableMissing([[maybe_unused]] const QString& path)
{
    MessageOverlay::CriticalAsync(kExeMissingTitle, kExeMissingMessage);
}

void RaytracingView::OnTraceFailed()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Error(GetModuleName().toStdString() + " capture failed.", 0, 0);
    }

    OnTraceEnded();
}

void RaytracingView::OnTraceAborted()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Warning(GetModuleName().toStdString() + " capture was aborted.", 0, 0);
    }

    OnTraceEnded();
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void RaytracingView::OnTraceEnded()
{
    ReloadFiles();

    OnEnableCaptureUi();
}

void RaytracingView::OnUiStatusChanged(const bool should_enable) const
{
    ui_->collect_data_button->setEnabled(should_enable);
}
