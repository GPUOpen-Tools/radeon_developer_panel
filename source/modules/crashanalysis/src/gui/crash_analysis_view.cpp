// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Crash analysis module client view class implementation.

#include "crash_analysis_view.h"

#include <common/inc/definitions.h>
#include <common/inc/file_client_view.h>

#include <common/inc/rdf_file_dialog.h>
#include <common/inc/util.h>

#include <qt_common/custom_widgets/message_overlay.h>

#include "crash_analysis_error_dialog.h"
#include "crash_analysis_module_definitions.h"

#include "ui_crash_analysis_aux_view.h"

// Adding a hidden view to a layout will still add the empty spacing for that view, which we want to avoid. To remedy this, we remove and insert views as they
// are hidden/shown. This is the index where views should be inserted to be above the utility view but below the status label.
static constexpr int kViewInsertionIndex = 2;

static constexpr auto kDisplayName = "Crash Analysis";

CrashAnalysisView::CrashAnalysisView(QWidget* utility_view, const std::shared_ptr<CrashAnalysisViewModel>& view_model, QWidget* parent)
    : SplitClientFileView(kDisplayName, kFileConceptName, kRgdFileExtension, parent)
    , view_model_(view_model)
    , aux_widget_ui_(new Ui::CrashAnalysisAuxWidget)
    , aux_widget_(new QWidget(this))
{
    aux_widget_ui_->setupUi(aux_widget_);
    aux_widget_->hide();

    // Setup timer to auto-hide the "no crash detected" message after 5 seconds.
    no_crash_detected_hide_timer_.setSingleShot(true);
    no_crash_detected_hide_timer_.setInterval(5000);
    connect(&no_crash_detected_hide_timer_, &QTimer::timeout, this, [this]() {
        RemoveContentWidget(aux_widget_);
        aux_widget_->hide();
    });

    // Add the utility view to the content area (settings panel on the left)
    AddContentWidget(utility_view);

    SetupConnections();

    file_client_view_->SetEnableOpenContextOption(false);

    file_client_view_->AddCustomDropdownOption(
        "Open text summary",
        [](const QString& path) { return FileExistsWithOtherExtension(path, "txt"); },
        [&](const QString& path) { OpenFileInTextEditor(path, "txt"); },
        false,
        true,
        true);

    file_client_view_->AddCustomDropdownOption(
        "Generate and open text summary",
        [](const QString& path) { return !FileExistsWithOtherExtension(path, "txt"); },
        [&](const QString& path) { QueueSummaryGeneration(path, true, false); },
        false,
        true,
        true);

    file_client_view_->AddCustomDropdownOption(
        "Open JSON summary",
        [](const QString& path) { return FileExistsWithOtherExtension(path, "json"); },
        [&](const QString& path) { OpenFileInTextEditor(path, "json"); },
        false,
        false,
        true);

    file_client_view_->AddCustomDropdownOption(
        "Generate and open JSON summary",
        [](const QString& path) { return !FileExistsWithOtherExtension(path, "json"); },
        [&](const QString& path) { QueueSummaryGeneration(path, false, true); },
        false,
        false,
        true);

    if (getenv(kEnableRdfInspectorEnv) != nullptr)
    {
        file_client_view_->AddCustomDropdownOption(
            "Inspect file",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                const auto dialog = new RdfFileDialog("RGD File Chunks", path, this);
                dialog->show();
            });
        file_client_view_->AddCustomDropdownOption(
            "Inspect files",
            [](const QString&) -> bool { return true; },
            [this](const QString& path) {
                const auto dialog = new RdfFileDialog("RGD File Chunks", path, this);
                dialog->show();
            },
            true);
    }
    connect(aux_widget_ui_->show_error_button, &QPushButton::pressed, this, &CrashAnalysisView::ShowSummaryError);
}

CrashAnalysisView::~CrashAnalysisView() = default;

void CrashAnalysisView::SetUserdataViewModel(const std::shared_ptr<CrashAnalysisUserdataViewModel>& userdata_view_model)
{
    userdata_view_model_ = userdata_view_model;

    SplitClientFileView::SetUserdataViewModel(userdata_view_model);
}

void CrashAnalysisView::SetupConnections()
{
    model_binder_.StartBinding();

    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    // Base status signals
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ConnectedProcessTextChanged, this, &SplitClientView::SetConnectedProcessText);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ClientStatusChanged, this, &SplitClientView::SetStatus);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::OutputPathInfoChanged, this, &SplitClientFileView::OnOutputPathChanged);

    // File operations
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::ExecutableMissing, this, &CrashAnalysisView::OnApplicationExecutableMissing);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::TextEditorMissing, this, &CrashAnalysisView::OnTextEditorMissing);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::TraceComplete, this, &CrashAnalysisView::OnTraceEnded);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::TraceFailed, this, &CrashAnalysisView::OnTraceEnded);
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::TraceAborted, this, &CrashAnalysisView::OnTraceEnded);

    // File client view connections
    model_binder_.Connect(file_client_view_.get(), &FileClientView::OpenFile, view_model.get(), &CrashAnalysisViewModel::OnOpenFile);
    model_binder_.Connect(file_client_view_.get(), &FileClientView::RemovedFile, view_model.get(), &CrashAnalysisViewModel::OnRemovedFile);

    // Crash analysis specific signals
    model_binder_.Connect(view_model.get(), &CrashAnalysisViewModel::AccessoryInfoChanged, this, &CrashAnalysisView::OnAccessoryInfoChanged);
    model_binder_.Connect(view_model.get(), &CrashAnalysisViewModel::IsCurrentHardwareApuChanged, this, &CrashAnalysisView::OnIsCurrentHardwareApuChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisViewModel::HardwareCrashAnalysisSupportedChanged, this, &CrashAnalysisView::OnHardwareCrashAnalysisSupportedChanged);

    // Forward CurrentConnectionsChanged to determine application connected state
    model_binder_.Connect(view_model.get(), &TraceSourceViewModel::CurrentConnectionsChanged, this, &CrashAnalysisView::OnCurrentConnectionsChanged);

    // Request UI sync with model
    view_model->Update();
}

void CrashAnalysisView::OnAccessoryInfoChanged(const CrashAnalysisAccessoryInfo& info)
{
    switch (info.type)
    {
    case CrashAnalysisAccessoryType::kNone:
        no_crash_detected_hide_timer_.stop();
        RemoveContentWidget(aux_widget_);
        aux_widget_->hide();
        break;
    case CrashAnalysisAccessoryType::kNoCrashDetected:
        aux_widget_ui_->crash_summary_error_page->hide();
        aux_widget_ui_->no_tdr_page->show();
        ShowAuxWidget();
        no_crash_detected_hide_timer_.start();
        break;
    case CrashAnalysisAccessoryType::kSummaryError:
        no_crash_detected_hide_timer_.stop();
        aux_widget_ui_->crash_summary_error_page->show();
        aux_widget_ui_->no_tdr_page->hide();
        aux_widget_ui_->show_error_button->setVisible(!info.error_string.isEmpty());
        ShowAuxWidget();
        break;
    }
}

void CrashAnalysisView::OnIsCurrentHardwareApuChanged(const bool is_apu)
{
    if (is_apu)
    {
        UpdateStatusDescription("APU hardware detected", "BSOD may occur during capture (turning off Secure Boot may improve capture stability).");
    }
    else
    {
        UpdateStatusDescription("", "");
    }
}

void CrashAnalysisView::OnHardwareCrashAnalysisSupportedChanged(const bool supported)
{
    if (!supported)
    {
        UpdateStatusDescription("Hardware crash analysis not supported",
                                "Hardware crash analysis is not supported with the current driver or hardware configuration.");
    }
}

void CrashAnalysisView::ShowSummaryError() const
{
    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    const QString error = view_model->GetAccessoryErrorString();
    if (error.isEmpty())
    {
        return;
    }

    CrashAnalysisErrorDialog error_dialog;
    error_dialog.setWindowTitle("Failure generating crash summaries");
    error_dialog.SetErrorMessage(error);
    error_dialog.exec();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void CrashAnalysisView::OnApplicationExecutableMissing(const QString& path)
{
    MessageOverlay::CriticalAsync(kRgdExeMissingDialogTitle, QString(kRgdExeMissingDialogMessage).arg(path), QString("continuous-missing-exe-%1").arg(path));
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void CrashAnalysisView::OnTextEditorMissing(const QString& path)
{
    MessageOverlay::CriticalAsync(kTxtEditorExeMissingDialogTitle, QString(kTxtEditExeMissingDialogMessage).arg(path));
}

void CrashAnalysisView::OnTraceEnded()
{
    ReloadFiles();
}

void CrashAnalysisView::OpenFileInTextEditor(const QString& path, const QString& extension) const
{
    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const QString modified_path = Util::GetPathForFileWithDifferentExtension(path, extension);
    view_model->OnOpenFileWithTextEditor(modified_path);
}

void CrashAnalysisView::QueueSummaryGeneration(const QString& path, const bool generate_text, const bool generate_json) const
{
    const auto view_model = view_model_.lock();
    Q_ASSERT(view_model != nullptr);

    view_model->QueueSummaryGeneration(path, generate_text, generate_json);
}

bool CrashAnalysisView::FileExistsWithOtherExtension(const QString& path, const QString& extension)
{
    const QFileInfo file_info(Util::GetPathForFileWithDifferentExtension(path, extension));
    const bool      file_exists = file_info.exists();

    return file_exists && file_info.size() > 0;
}

void CrashAnalysisView::ShowAuxWidget()
{
    InsertContentWidget(aux_widget_, kViewInsertionIndex);
    aux_widget_->show();
}

void CrashAnalysisView::OnCurrentConnectionsChanged([[maybe_unused]] std::unordered_map<DDConnectionId, devtrace::Api> connections)
{
    // Application is connected when pid != 0
    const auto view_model = view_model_.lock();
    if (view_model == nullptr)
    {
        return;
    }

    const auto& status    = view_model->GetSourceStatus();
    const bool  connected = (status.pid != 0);

    const auto userdata_model = userdata_view_model_.lock();
    if (userdata_model != nullptr)
    {
        userdata_model->HandleApplicationConnectedChanged(connected);
    }
}
