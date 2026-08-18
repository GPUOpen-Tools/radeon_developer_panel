// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing module view model implementation.

#include "raytracing_view_model.h"

#include <QSettings>
#include <QStandardPaths>

#include <logging.h>
#include <source_userdata.h>
#include <source_userdata_mapper.h>
#include <trace_source_factory.h>

#include <common/inc/definitions.h>
#include <common/inc/util.h>

#include "raytracing_module_definitions.h"
#include "raytracing_userdata_view_model.h"
#include "utility/rra_trace_validator.h"

bool RaytracingViewModel::ReceiveUserData(const std::string& data)
{
    devtrace::RraUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRraScenesDefaultParentFolder);

    if (devtrace::RraUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata).has_value())
    {
        if (trace_source_ != nullptr)
        {
            const char* raw_buffer_size = devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes[userdata.ray_history_buffer_size_index];

            char*    end;
            uint64_t buffer_size = strtoull(raw_buffer_size, &end, 10);

            if (buffer_size == 0)
            {
                buffer_size =
                    strtoull(devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes[RayHistoryBufferSizeIndex::kRayHistoryBufferIndexDefaultBuffer], &end, 10);
            }

            trace_source_->GetConfig().ray_history_buffer_size = buffer_size;
        }

        SetOutputPath(userdata.output_path.c_str());
        return true;
    }

    logger_->Error("Failed to parse userdata", 0, 0);
    SetOutputPath(default_output_path);

    return false;
}

void RaytracingViewModel::OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args)
{
    auto*      self  = static_cast<RaytracingViewModel*>(object);
    const auto stage = args.new_status.GetStage();

    // Reset delay capture flag once actual capture begins
    if (stage == devtrace::TraceSourceStage::kCapturing || stage == devtrace::TraceSourceStage::kProcessing || stage == devtrace::TraceSourceStage::kDumping)
    {
        self->is_delay_capture_in_progress_ = false;
    }

    if (stage == devtrace::TraceSourceStage::kCapturing || stage == devtrace::TraceSourceStage::kProcessing || stage == devtrace::TraceSourceStage::kDumping ||
        self->is_delay_capture_in_progress_)
    {
        self->logger_->Info("Showing progress UI", 0, 0);
        emit self->ShowProgressUi();
    }
    else
    {
#ifndef NDEBUG
        self->logger_->Info("Showing capture UI", 0, 0);
#endif
        emit self->ShowCaptureUi();
    }
}

void RaytracingViewModel::OnRraTraceSupportEvent(void* object, const devtrace::RraTraceSourceSupportEventArgs& args)
{
    auto* self = static_cast<RaytracingViewModel*>(object);
    emit  self->TraceSupportChanged(args);
}

bool RaytracingViewModel::ShouldEnableUiForTraceStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kIdle;
}

bool RaytracingViewModel::ShouldProgressBarBeShownForStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kWaitingToBeginCapture || stage == devtrace::TraceSourceStage::kCapturing ||
           stage == devtrace::TraceSourceStage::kDumping || stage == devtrace::TraceSourceStage::kProcessing;
}

RaytracingViewModel::RaytracingViewModel(const std::shared_ptr<FileSystemStreamProvider>&    stream_provider,
                                         const std::shared_ptr<devtrace::RraTraceSource>&    rra_trace_source,
                                         const std::shared_ptr<FileUtils>&                   file_utils,
                                         const std::shared_ptr<TraceFileOpener>&             file_opener,
                                         QSettings*                                          tool_settings,
                                         const std::shared_ptr<RaytracingUserdataViewModel>& userdata_view_model,
                                         const std::shared_ptr<devtrace::TraceTimer>&        delay_timer,
                                         const std::shared_ptr<MercuryLogger>&               logger)
    : TraceSourceViewModel(stream_provider, rra_trace_source, file_utils, file_opener, kRraFileExtension, tool_settings, logger)
    , trace_source_(rra_trace_source)
    , userdata_view_model_(userdata_view_model)
    , delay_timer_(delay_timer)
{
    Q_ASSERT(trace_source_ != nullptr);

    delay_timer_->SetSingleShot(true);
    delay_timer_->SetOnTimerFire([&] {
        is_delay_capture_in_progress_ = false;
        if (trace_source_->RequestBeginTrace(delayed_capture_connection_id_) != devtrace::Result::kSuccess)
        {
            logger_->Error("Delay Timer: There was an error making the request to begin tracing.", 0, 0);
        }
    });

    trace_source_->RegisterStatusEvent({this, OnStatusEventCallback});
    trace_source_->RegisterTraceCaptureProgressEvent({this, OnTraceCaptureProgressEventCallback});
    trace_source_->RegisterSupportEvent({this, OnRraTraceSupportEvent});
}

void RaytracingViewModel::Update() const
{
    trace_source_->QueryStatus();
}

void RaytracingViewModel::RequestBeginTrace()
{
    const DDConnectionId capture_target = GetCaptureTarget();
    if (trace_source_ == nullptr || capture_target == 0)
    {
        return;
    }

    const auto& config = trace_source_->GetConfig();
    logger_->Info("RRA trace config: ray_history_buffer_size=" + std::to_string(config.ray_history_buffer_size) + ", enable_ray_history=" +
                      std::to_string(config.enable_ray_history.load()) + ", enable_marker_capture=" + std::to_string(config.enable_marker_capture.load()) +
                      ", marker_begin_string=" + config.marker_begin_string + ", marker_end_string=" + config.marker_end_string,
                  0,
                  0);

    const auto [delay_enabled, delay_ms] = userdata_view_model_->GetDelayInfo();
    if (!delay_enabled)
    {
        if (trace_source_->RequestBeginTrace(capture_target) != devtrace::Result::kSuccess)
        {
            logger_->Error("There was an error making the request to begin tracing.", 0, 0);
        }
        return;
    }

    if (trace_source_->PrepareForDelayedCapture(capture_target) != devtrace::Result::kSuccess)
    {
        logger_->Error("Failed to prepare the trace source for a delayed capture.", 0, 0);
        return;
    }

    delayed_capture_connection_id_ = capture_target;

    // Set flag to indicate delay capture is in progress
    is_delay_capture_in_progress_ = true;

    // Show progress bar with infinite indicator during delay
    emit ShowProgressUi();
    emit ProgressInfoStep(ProgressInfo{.show_progress_bar = true, .can_cancel = true, .progress = 0.0F, .progress_text = "Waiting for delayed capture..."});

    delay_timer_->SetInterval(static_cast<int>(delay_ms));
    delay_timer_->Start();
}

void RaytracingViewModel::RequestAbort()
{
    delay_timer_->Stop();
    is_delay_capture_in_progress_ = false;

    TraceSourceViewModel::RequestAbort();
}

void RaytracingViewModel::RequestAbort(DDConnectionId umd_connection_id)
{
    delay_timer_->Stop();
    is_delay_capture_in_progress_ = false;

    trace_source_->RequestAbortTrace(umd_connection_id);
}

QString RaytracingViewModel::GetProcessingText()
{
    return "";
}

QString RaytracingViewModel::GetToolApplicationPath() const
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("rra_path"))
    {
        return "";
    }

    return tool_settings_->value("rra_path").toString();
}

void RaytracingViewModel::TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, const bool file_deleted)
{
    Q_UNUSED(file_deleted)

    if (trace_source_ == nullptr)
    {
        return;
    }

    bool validation_failed = false;
    if (status == devtrace::TraceCompletionStatus::kCompleted)
    {
        const QString                  file_path         = QString::fromStdString(path);
        const RraTraceValidationResult validation_result = RraTraceValidator::ValidateFile(file_path, this->userdata_view_model_->IsRayHistoryEnabled());
        validation_failed = validation_result != RraTraceValidationResult::kSuccess && validation_result != RraTraceValidationResult::kFailedToValidate;

        if (validation_failed)
        {
            file_utils_->RemoveFile(file_path);
            switch (validation_result)
            {
            case RraTraceValidationResult::kMissingBvh:
                emit CaptureMissingBvhData();
                break;
            case RraTraceValidationResult::kMissingRayHistory:
                emit CaptureMissingRayHistory();
                break;
            case RraTraceValidationResult::kIncompleteRayHistory:
                emit CaptureIncompleteRayHistory();
                break;
            default:
                break;
            }
        }
    }

    TraceSourceViewModel::TraceCompleted(status, path, validation_failed);
}
