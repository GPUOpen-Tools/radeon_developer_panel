// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a base view model to manipulate a trace source.

#include "model/trace_source_view_model.h"

#include <functional>

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <utility>

#include <source_status.h>

#include <qt_common/utils/qt_util.h>

#include "definitions.h"
#include "util.h"

static constexpr float       kProgressStep      = 0.01f;
static constexpr const char* kAutoOpenTraceKey  = "auto_open_traces";
static constexpr const char* kRgpBackendTestKey = "rgp_backend_test_path";
static constexpr const char* kRmvBackendTestKey = "rmv_backend_test_path";
static constexpr const char* kRraBackendTestKey = "rra_backend_test_path";
static constexpr const char* kRgpBackendTestArg = "";
static constexpr const char* kRmvBackendTestArg = "--rmv";
static constexpr const char* kRraBackendTestArg = "--rra";

namespace
{

    QColor ColorForStage(const devtrace::TraceSourceStage stage)
    {
        switch (stage)
        {
        case devtrace::TraceSourceStage::kIdle:
        case devtrace::TraceSourceStage::kWaitingToBeginCapture:
        case devtrace::TraceSourceStage::kCapturing:
        case devtrace::TraceSourceStage::kDumping:
        case devtrace::TraceSourceStage::kProcessing:
        case devtrace::TraceSourceStage::kDone:
            return {"darkGreen"};
        case devtrace::TraceSourceStage::kDisabled:
            return Qt::darkYellow;
        case devtrace::TraceSourceStage::kDisconnected:
        case devtrace::TraceSourceStage::kError:
        case devtrace::TraceSourceStage::kBusy:
        default:
            return {"red"};
        }
    }

    QString GetConnectedProcessText(const devtrace::TraceSourceStatus& status)
    {
        const std::string& app_name = status.application_name;
        if (app_name.empty())
        {
            return {""};
        }

        return QString("%1 PID: %2").arg(app_name.c_str()).arg(static_cast<int>(status.pid));
    }

}  // namespace

ProgressInfo TraceSourceViewModel::ProcessProgressInfoFromCaptureProgressEventArgs(const devtrace::TraceCaptureProgressEventArgs& args)
{
    const devtrace::TraceSourceStatus& status = args.new_progress.status;
    const auto                         stage  = status.GetStage();
    if (!ShouldProgressBarBeShownForStage(stage))
    {
        return {.show_progress_bar = false, .can_cancel = false, .progress = 0.0, .progress_text = ""};
    }

    // Some models may elect not to show the progress bar for all stages, but the logic for each stage
    // should be the same regardless of which they show.
    if (stage == devtrace::TraceSourceStage::kWaitingToBeginCapture)
    {
        // Use progress = 0.0 to show infinite/indeterminate progress bar during the delay
        return {.show_progress_bar = true,
                .can_cancel        = status.abort_trace_supported,
                .progress          = 0.0,
                .progress_text     = QString("Waiting for capture to begin...")};
    }

    if (stage == devtrace::TraceSourceStage::kCapturing)
    {
        return {.show_progress_bar = true,
                .can_cancel        = status.abort_trace_supported,
                .progress          = status.stage_progress,
                .progress_text     = QString(kTraceProgressReceived).arg("-", "-")};
    }

    if (stage == devtrace::TraceSourceStage::kDumping)
    {
        QString size_string;
        QtCommon::QtUtils::GetFilesizeAcronymFromByteCount(status.num_bytes_dumped, size_string);

        QString total_size_string;
        QtCommon::QtUtils::GetFilesizeAcronymFromByteCount(status.total_bytes_to_dump, total_size_string);

        return {.show_progress_bar = true,
                .can_cancel        = status.abort_trace_supported,
                .progress          = status.stage_progress,
                .progress_text     = QString(kTraceProgressReceived).arg(size_string, total_size_string)};
    }

    if (stage == devtrace::TraceSourceStage::kProcessing)
    {
        return {.show_progress_bar = true,
                .can_cancel        = status.abort_processing_supported,
                .progress          = status.stage_progress,
                .progress_text     = args.new_progress.progress_text.c_str()};
    }

    return {.show_progress_bar = true, .can_cancel = false, .progress = status.stage_progress, .progress_text = ""};
}

void TraceSourceViewModel::OnStatusEventCallback(void* listener, const devtrace::TraceSourceStatusEventArgs& args)
{
    auto* self = static_cast<TraceSourceViewModel*>(listener);

    self->current_status_ = args.new_status;

    // Emit client status update
    const auto stage        = args.new_status.GetStage();
    const auto status_text  = devtrace::TraceSourceStageToString(stage);
    const auto status_color = ColorForStage(stage);
    emit       self->ClientStatusChanged({.text = status_text.c_str(), .color = status_color});

    // Emit process text update
    const auto process_text = GetConnectedProcessText(args.new_status);
    emit       self->ConnectedProcessTextChanged(process_text);

    // Emit Ui enablement update
    emit self->UiStatusChanged(self->ShouldEnableUiForTraceStage(stage));

    // Emit disabled reason update
    const auto reason_description = QString::fromStdString(devtrace::DisabledReasonToString(args.new_status.disabled_reason));
    emit       self->DisabledReasonChanged(reason_description);

    self->connected_app_name_ = QString::fromStdString(args.new_status.application_name);
#ifndef NDEBUG
    self->logger_->Info("Connected application name: " + self->connected_app_name_.toStdString(), 0, 0);
#endif

    self->capture_target_ = args.new_status.current_connections.empty() ? 0 : args.new_status.current_connections.begin()->first;
    if (self->current_connections_cache_ != args.new_status.current_connections)
    {
        self->current_connections_cache_ = args.new_status.current_connections;
        emit self->CurrentConnectionsChanged(self->current_connections_cache_);
    }
}

void TraceSourceViewModel::OnTraceCompletionEventCallback(void* listener, const devtrace::TraceCompletionEventArgs& args)
{
    if (args.result.status == devtrace::TraceCompletionStatus::kNeedProcessing)
    {
        // Ignore completion results that indicate additional processing is required as
        // once the additional trace processing is finished, a new completion event should be fired.
        return;
    }

    auto* self = static_cast<TraceSourceViewModel*>(listener);

    self->TraceCompleted(args.result.status, args.result.path, false);

    // Emit a progress info step to reset the UI for next trace
    emit self->ProgressInfoStep(ProgressInfo{.show_progress_bar = false, .can_cancel = false, .progress = 1.0F, .progress_text = ""});
}

void TraceSourceViewModel::OnTraceCaptureProgressEventCallback(void* listener, const devtrace::TraceCaptureProgressEventArgs& args)
{
    auto* self = static_cast<TraceSourceViewModel*>(listener);

    const ProgressInfo info = self->ProcessProgressInfoFromCaptureProgressEventArgs(args);
    emit               self->ProgressInfoStep(info);
}

TraceSourceViewModel::TraceSourceViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                                           const std::shared_ptr<devtrace::TraceSource>&    source,
                                           const std::shared_ptr<FileUtils>&                file_utils,
                                           const std::shared_ptr<TraceFileOpener>&          file_opener,
                                           QString                                          file_extension,
                                           QSettings*                                       tool_settings,
                                           const std::shared_ptr<MercuryLogger>&            logger)
    : extension_(std::move(file_extension))
    , stream_provider_(stream_provider)
    , file_opener_(file_opener)
    , source_(source)
    , file_utils_(file_utils)
    , logger_(logger)
    , tool_settings_(tool_settings)
    , display_application_name_("")
    , raw_output_path_("")
    , output_info_({})
    , capture_target_(0)
{
    qRegisterMetaType<CurrentConnections>();

    // If the source is null, that should still be able to be gracefully handled.
    if (source_ == nullptr)
    {
        return;
    }

    source_->RegisterStatusEvent({.listener = this, .callback = OnStatusEventCallback});
    source_->RegisterTraceCompletionEvent({.listener = this, .callback = OnTraceCompletionEventCallback});
    source_->RegisterTraceCaptureProgressEvent({.listener = this, .callback = OnTraceCaptureProgressEventCallback});
}

void TraceSourceViewModel::SetDisplayApplicationName(const QString& application_name)
{
    const std::scoped_lock lock(output_path_mutex_);

    display_application_name_ = application_name;

    const QString display_path = Util::ExpandOutputPathMacros(raw_output_path_, display_application_name_);
    file_utils_->CreateFolder(display_path);

    const QString trace_output_path = application_name.isEmpty() ? display_path : Util::ExpandOutputPathMacros(raw_output_path_, connected_app_name_);
    file_utils_->CreateFolder(trace_output_path);

    stream_provider_->SetAppName(connected_app_name_.isEmpty() ? application_name : connected_app_name_);
    stream_provider_->SetPath(trace_output_path);

    output_info_ = {.expanded_path = display_path, .raw_path = raw_output_path_};
    emit OutputPathInfoChanged(output_info_);
}

uint16_t TraceSourceViewModel::GetCaptureTarget() const
{
    return capture_target_;
}

void TraceSourceViewModel::SetCaptureTarget(uint16_t capture_target)
{
    capture_target_ = capture_target;
    emit CaptureTargetChanged(capture_target);
}

const devtrace::TraceSourceStatus& TraceSourceViewModel::GetSourceStatus() const
{
    return current_status_;
}

const std::shared_ptr<MercuryLogger>& TraceSourceViewModel::GetLogger() const
{
    return logger_;
}

QString TraceSourceViewModel::GetProcessingText()
{
    return "Processing";
}

void TraceSourceViewModel::OnOpenFile(const QString& path)
{
    OpenFile(path);
}

void TraceSourceViewModel::OpenFile(const QString& path)
{
    switch (const QString exe_path = GetToolApplicationPath(); file_opener_->Open(path, exe_path))
    {
    case TraceFileOpenerResult::kMissingExecutable:
        emit ExecutableMissing(exe_path);
        break;
    case TraceFileOpenerResult::kFailedToLaunch:
        logger_->LogError("Failed to launch the tool with trace {}", 0, 0, path.toStdString());
        break;
    default:
        break;
    }
}

void TraceSourceViewModel::OnOpenFileWithTextEditor(const QString& path)
{
    switch (const QString exe_path = GetTextEditorApplicationPath(); file_opener_->Open(path, exe_path))
    {
    case TraceFileOpenerResult::kMissingExecutable:
        emit TextEditorMissing(exe_path);
        break;
    case TraceFileOpenerResult::kFailedToLaunch:
        logger_->LogError("Failed to launch text editor with file {}", 0, 0, path.toStdString());
        break;
    default:
        break;
    }
}

void TraceSourceViewModel::OnRemovedFile(const QString& path)
{
    RemovedFile(path);
}

void TraceSourceViewModel::RemovedFile(const QString& path)
{
    Q_UNUSED(path)
}

void TraceSourceViewModel::RequestAbort()
{
    RequestAbort(capture_target_);
}

void TraceSourceViewModel::RequestAbort(DDConnectionId umd_connection_id)
{
    if (source_ == nullptr)
    {
        return;
    }

    const devtrace::TraceSourceStatus status = GetSourceStatus();
    if (status.abort_trace_supported)
    {
        if (source_->RequestAbortTrace(umd_connection_id) != devtrace::Result::kSuccess)
        {
            logger_->Warning("Failed to request aborting a trace", 0, 0);
        }
    }

    if (status.abort_processing_supported)
    {
        if (source_->RequestAbortProcessing() != devtrace::Result::kSuccess)
        {
            logger_->Warning("Failed to request aborting processing", 0, 0);
        }
    }
}

void TraceSourceViewModel::SetOutputPath(const QString& path)
{
    const std::scoped_lock lock(output_path_mutex_);

    raw_output_path_ = path;
}

QString TraceSourceViewModel::GetTextEditorApplicationPath()
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("txt_editor_path"))
    {
        return "";
    }

    return tool_settings_->value("txt_editor_path").toString();
}

QString TraceSourceViewModel::GetBackendTestApplicationPath() const
{
    if (tool_settings_ == nullptr)
    {
        return "";
    }

    // Decide which backend test executable key to use based on extension
    const QString ext_lower = extension_.toLower();
    const char*   key       = nullptr;
    if (ext_lower == "rgp")
    {
        key = kRgpBackendTestKey;
    }
    else if (ext_lower == "rmv")
    {
        key = kRmvBackendTestKey;
    }
    else if (ext_lower == "rra")
    {
        key = kRraBackendTestKey;
    }

    if (key == nullptr || !tool_settings_->contains(key))
    {
        return "";
    }

    return tool_settings_->value(key).toString();
}

QString TraceSourceViewModel::GetBackendTestApplicationArgs() const
{
    // Decide which backend test executable arguments to use based on extension
    const QString ext_lower = extension_.toLower();
    const char*   arg       = nullptr;
    if (ext_lower == "rgp")
    {
        arg = kRgpBackendTestArg;
    }
    else if (ext_lower == "rmv")
    {
        arg = kRmvBackendTestArg;
    }
    else if (ext_lower == "rra")
    {
        arg = kRraBackendTestArg;
    }

    if (arg == nullptr)
    {
        return "";
    }

    return QString(arg);
}

void TraceSourceViewModel::OnBackendTestRequested(const QString& path)
{
    const QFileInfo file_info(path);
    if (!file_info.exists())
    {
        logger_->LogWarning("Backend test requested on non-existent file {}", 0, 0, path.toStdString());
        return;
    }

    const QString exe_path = GetBackendTestApplicationPath();
    if (exe_path.isEmpty())
    {
        emit BackendTestExecutableMissing(exe_path);
        return;
    }

    const QString arguments = GetBackendTestApplicationArgs();

    QFileInfo exe_info(exe_path);
    if (!exe_info.exists() || !exe_info.isExecutable())
    {
        emit BackendTestExecutableMissing(exe_path);
        return;
    }

    QProcess* process = new QProcess();
    process->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(
        process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process, [&, path, process](int exit_code, QProcess::ExitStatus status) {
            QByteArray output_bytes = process->readAll();
            QString    output_str   = QString::fromLocal8Bit(output_bytes);
            if (status == QProcess::NormalExit && exit_code >= 0)
            {
                emit BackendTestSucceeded(path, output_str);
            }
            else
            {
                emit BackendTestFailed(path, exit_code, output_str);
            }
            process->deleteLater();
        });

    QStringList args{arguments, QDir::toNativeSeparators(path)};
    process->start(exe_path, args);
    if (!process->waitForStarted())
    {
        // Process failed to start
        emit BackendTestFailed(path, -1, "Failed to start backend test executable.");
        process->deleteLater();
    }
}

void TraceSourceViewModel::TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, bool file_deleted)
{
    if (status != devtrace::TraceCompletionStatus::kCompleted)
    {
        if (status != devtrace::TraceCompletionStatus::kAborted)
        {
            if (!file_deleted)
            {
                file_utils_->RemoveFile(path.c_str());
            }

            emit TraceFailed();
        }
        else
        {
            emit TraceAborted();
        }

        return;
    }

    if (!file_deleted && ShouldAutoOpen())
    {
        OnOpenFile(path.c_str());
    }

    emit TraceComplete();
}

bool TraceSourceViewModel::ShouldAutoOpen() const
{
    return tool_settings_ != nullptr && tool_settings_->value(kAutoOpenTraceKey).toBool();
}
