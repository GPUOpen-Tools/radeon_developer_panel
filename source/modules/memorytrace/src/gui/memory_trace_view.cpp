// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory Trace View implementation

#include "memory_trace_view.h"

#include "ui_memory_trace_capture.h"

#include <common/inc/definitions.h>
#include <common/inc/file_client_view.h>

#include <common/inc/rdf_file_dialog.h>

#include "common/inc/collapsible_pane.h"
#include "memory_trace_module_definitions.h"
#include "qt_common/custom_widgets/message_overlay.h"

static constexpr auto kDisplayName       = "Memory Trace";
constexpr auto        kExeMissingTitle   = "Radeon Memory Visualizer missing";
constexpr auto        kExeMissingMessage = "Radeon Memory Visualizer executable not found. Please specify path in settings.";

MemoryTraceView::MemoryTraceView([[maybe_unused]] QWidget* utility_view, const std::shared_ptr<MemoryTraceViewModel>& view_model, QWidget* parent)
    : SplitClientFileView(kDisplayName, kFileConceptName, kRmvFileExtension, parent)
    , capture_ui_(new Ui::MemoryTraceCapture)
    , view_model_(view_model)
{
    current_connection_model_ = new CurrentConnectionModel(this);

    auto* capture_widget = new CollapsiblePane<QWidget>(this);
    capture_widget->SetTitleText("Capture");
    capture_ui_->setupUi(capture_widget->GetBody());
    AddContentWidget(capture_widget);
    SetupConnections();

    if (getenv(kEnableRdfInspectorEnv) != nullptr)
    {
        file_client_view_->AddCustomDropdownOption(
            "Inspect file",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                const auto dialog = new RdfFileDialog("RMV File Chunks", path, this);
                dialog->show();
            });
        file_client_view_->AddCustomDropdownOption(
            "Inspect files",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                const auto dialog = new RdfFileDialog("RMV File Chunks", path, this);
                dialog->show();
            },
            true);
    }
}

MemoryTraceView::~MemoryTraceView() = default;

void MemoryTraceView::SetupConnections()
{
    model_binder_.StartBinding();

    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    capture_ui_->capture_progress_widget->UpdateProgress({.progress = 0.5f, .progress_text = ""});
    model_binder_.Connect(capture_ui_->dump_button, &QPushButton::clicked, view_model.get(), &MemoryTraceViewModel::RequestDump);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ConnectedProcessTextChanged, this, &SplitClientView::SetConnectedProcessText);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ClientStatusChanged, this, &SplitClientView::SetStatus);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::CurrentConnectionsChanged, this, &MemoryTraceView::OnCurrentConnectionsChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::UiStatusChanged, this, &MemoryTraceView::OnUiStatusChanged);
    model_binder_.Connect(
        view_model.get(), &TraceSourceViewModel::ProgressInfoStep, capture_ui_->capture_progress_widget, &CaptureProgressWidget::UpdateProgress);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::ExecutableMissing, this, &MemoryTraceView::OnApplicationExecutableMissing);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::TraceComplete, this, &MemoryTraceView::OnTraceEnded);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::TraceFailed, this, &MemoryTraceView::OnTraceFailed);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::TraceAborted, this, &MemoryTraceView::OnTraceAborted);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::ShowCaptureUi, this, &MemoryTraceView::OnEnableCaptureUi);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::ShowProgressUi, this, &MemoryTraceView::OnEnableProgressUi);
    model_binder_.Connect(view_model.get(), &MemoryTraceViewModel::OutputPathInfoChanged, this, &SplitClientFileView::OnOutputPathChanged);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::DisabledReasonChanged, this, &SplitClientView::UpdateUnsupportedDescription);

    model_binder_.Connect(file_client_view_.get(), &FileClientView::OpenFile, view_model.get(), &TraceSourceViewModel::OnOpenFile);
    model_binder_.Connect(file_client_view_.get(), &FileClientView::RemovedFile, view_model.get(), &TraceSourceViewModel::OnRemovedFile);

    connect(capture_ui_->insert_snapshot, &QPushButton::clicked, this, &MemoryTraceView::OnInsertMarker);

    capture_ui_->active_connection_selection->setModel(current_connection_model_);

    // Set initial state - combo box should be disabled when there are no connections
    capture_ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());

    // Request UI sync with model
    view_model->Update();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void MemoryTraceView::OnApplicationExecutableMissing([[maybe_unused]] const QString& path) const
{
    MessageOverlay::CriticalAsync(kExeMissingTitle, kExeMissingMessage);
}

void MemoryTraceView::OnTraceFailed()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Error(GetModuleName().toStdString() + " capture failed.", 0, 0);
    }

    OnTraceEnded();
}

void MemoryTraceView::OnInsertMarker()
{
    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    view_model->RequestMarker(capture_ui_->snapshot_name_edit->text());
    capture_ui_->snapshot_name_edit->clear();
}

void MemoryTraceView::OnTraceAborted()
{
    if (const auto view_model = view_model_.lock())
    {
        view_model->GetLogger()->Warning(GetModuleName().toStdString() + " capture was aborted.", 0, 0);
    }

    OnTraceEnded();
}

void MemoryTraceView::OnTraceEnded()
{
    ReloadFiles();

    OnEnableCaptureUi();
}

void MemoryTraceView::OnCurrentConnectionsChanged(const std::unordered_map<DDConnectionId, devtrace::Api>& connections) const
{
    current_connection_model_->OnCurrentConnectionsChanged(connections);
    capture_ui_->active_connection_selection->setEnabled(current_connection_model_->IsSelectionEnabled());
}

void MemoryTraceView::OnEnableCaptureUi() const
{
    capture_ui_->capture_widget_stack->setCurrentWidget(capture_ui_->collect_data_page);
    capture_ui_->capture_progress_widget->Reset();
}

void MemoryTraceView::OnEnableProgressUi() const
{
    capture_ui_->capture_widget_stack->setCurrentWidget(capture_ui_->progress_page);
}

void MemoryTraceView::OnUiStatusChanged(const bool should_enable) const
{
    capture_ui_->dump_button->setEnabled(should_enable);
    capture_ui_->insert_snapshot->setEnabled(should_enable);
    capture_ui_->snapshot_name_edit->setEnabled(should_enable);
}
