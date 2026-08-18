// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module model class implementation.

#include "profiling_view_model.h"

#include <algorithm>
#include <ranges>

#include <QSettings>
#include <QStandardPaths>

#include <counters/rgp_derived_spm_database.h>
#include <counters/rgp_spm_counter_handler.h>
#include <source_userdata.h>
#include <source_userdata_mapper.h>

#include <common/inc/definitions.h>

#include <common/inc/util.h>

#include "profiling_module_definitions.h"
#include "profiling_userdata_view_model.h"
#include "rgp_file_validator.h"

#include "spm_counter_file_parser.h"

namespace
{
    QString TextForCaptureMode(uint32_t capture_mode)
    {
        switch (static_cast<devtrace::RgpCaptureMode>(capture_mode))
        {
        case devtrace::RgpCaptureMode::kFrame:
            return "Frame";
        case devtrace::RgpCaptureMode::kDraw:
            return "Draw";
        case devtrace::RgpCaptureMode::kDispatch:
            return "Dispatch";
        default:
            break;
        }

        if (capture_mode == 0)
        {
            return "Automatic";
        }

        return "Unknown";
    }

}  // namespace

void ProfilingViewModel::ProcessStatusAvailableCaptureModes(const devtrace::TraceSourceStatusEventArgs& args)
{
    const DDConnectionId capture_target = args.new_status.current_connections.empty() ? 0 : args.new_status.current_connections.begin()->first;

    if (capture_target == 0)
    {
        available_capture_modes_ = {CaptureMode{"None", 0}};
        current_capture_mode_    = 0;

        emit AvailableCaptureModesChanged(available_capture_modes_);
        emit CurrentCaptureModeChanged(current_capture_mode_);

        return;
    }

    std::vector<uint32_t> raw_capture_modes;
    if (trace_source_ != nullptr)
    {
        trace_source_->GetSupportedCaptureModes(capture_target, raw_capture_modes);
    }

    std::vector<CaptureMode> capture_modes;
    bool                     current_capture_mode_available = false;

    for (const uint32_t raw_mode : raw_capture_modes)
    {
        // The automatic capture mode should always be present since it should always be supported. However, if there are additional explicit capture modes,
        // the automatic mode will just get mapped to those, so we don't need to show it in the options list.
        if (raw_mode == 0 && raw_capture_modes.size() > 1)
        {
            continue;
        }

        capture_modes.emplace_back(CaptureMode{TextForCaptureMode(raw_mode), raw_mode});
        current_capture_mode_available |= raw_mode == current_capture_mode_;
    }

    if (available_capture_modes_ != capture_modes)
    {
        available_capture_modes_ = capture_modes;
        emit AvailableCaptureModesChanged(available_capture_modes_);
    }

    if (!current_capture_mode_available)
    {
        const auto it  = args.new_status.current_connections.find(capture_target);
        const auto api = it != args.new_status.current_connections.end() ? it->second : devtrace::Api::kUnknown;

        current_capture_mode_ = GetDefaultCaptureMode(capture_modes, args.new_status.application_name, api);
        emit CurrentCaptureModeChanged(current_capture_mode_);
    }
}

bool ProfilingViewModel::ReceiveUserData(const std::string& data)
{
    devtrace::RgpUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRgpProfilesDefaultParentFolder);

    const auto default_path = default_output_path.toStdString();
    if (devtrace::RgpUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_path, userdata).has_value())
    {
        // Defense-in-depth: clamp dispatch_start_index >= 1 on load. The trace stack
        // (UberTrace render-op controller, legacy RGP client) treats 0 as "no trigger
        // registered", which would cause dispatch auto-capture to never fire.
        // The spinbox view enforces this minimum interactively, but a malformed/legacy
        // saved JSON could otherwise feed 0 directly into the trace source config below.
        if (userdata.dispatch_start_index < 1)
        {
            userdata.dispatch_start_index = 1;
        }

        SetOutputPath(userdata.output_path.c_str());

        if (trace_source_ != nullptr)
        {
            devtrace::RgpTraceSourceConfig& config = trace_source_->GetConfig();
            config.dispatch_start_index            = userdata.dispatch_start_index;
            config.dispatch_count                  = userdata.dispatch_count;
            config.auto_capture_mode               = userdata.auto_capture_mode;
            config.compute_auto_capture_time_ms    = userdata.compute_auto_capture_time_ms;

            if (const uint32_t sqtt_buffer_index = static_cast<uint32_t>(userdata.sqtt_buffer_size_profile);
                sqtt_buffer_index < devtrace::RgpTraceSourceConfig::kSqttProfileSizes.size())
            {
                config.sqtt_memory_limit = devtrace::RgpTraceSourceConfig::kSqttProfileSizes[sqtt_buffer_index];
            }
            else
            {
                // Use the driver default
                config.sqtt_memory_limit = 0;
            }

            config.frame_capture_index = userdata.frame_capture_index;

            default_capture_modes_ = userdata.default_capture_modes;
        }

        return true;
    }

    logger_->Error("Failed to parse userdata", 0, 0);
    SetOutputPath(default_output_path);

    return false;
}

void ProfilingViewModel::OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args)
{
    auto* self = static_cast<ProfilingViewModel*>(object);

    self->ProcessStatusAvailableCaptureModes(args);

    const auto stage = args.new_status.GetStage();

    // Reset delay capture flag once actual capture begins
    if (stage == devtrace::TraceSourceStage::kCapturing || stage == devtrace::TraceSourceStage::kProcessing || stage == devtrace::TraceSourceStage::kDumping)
    {
        self->is_delay_capture_in_progress_ = false;
    }

    if (stage == devtrace::TraceSourceStage::kWaitingToBeginCapture || stage == devtrace::TraceSourceStage::kCapturing ||
        stage == devtrace::TraceSourceStage::kProcessing || stage == devtrace::TraceSourceStage::kDumping || self->is_delay_capture_in_progress_)
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

void ProfilingViewModel::OnRgpTraceSourceSupportEvent(void* object, const devtrace::RgpTraceSourceSupportEventArgs& args)
{
    auto* self = static_cast<ProfilingViewModel*>(object);
    emit  self->TraceSupportChanged(args);
}

bool ProfilingViewModel::ShouldEnableUiForTraceStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kIdle;
}

bool ProfilingViewModel::ShouldProgressBarBeShownForStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kWaitingToBeginCapture || stage == devtrace::TraceSourceStage::kCapturing ||
           stage == devtrace::TraceSourceStage::kDumping || stage == devtrace::TraceSourceStage::kProcessing;
}

ProfilingViewModel::ProfilingViewModel(const std::shared_ptr<FileSystemStreamProvider>&   stream_provider,
                                       const std::shared_ptr<devtrace::RgpTraceSource>&   rgp_trace_source,
                                       const std::shared_ptr<FileUtils>&                  file_utils,
                                       const std::shared_ptr<TraceFileOpener>&            file_opener,
                                       QSettings*                                         tool_settings,
                                       const std::shared_ptr<ProfilingUserdataViewModel>& userdata_view_model,
                                       const std::shared_ptr<devtrace::TraceTimer>&       delay_timer,
                                       const std::shared_ptr<MercuryLogger>&              logger)
    : TraceSourceViewModel(stream_provider, rgp_trace_source, file_utils, file_opener, kProfileExtension, tool_settings, logger)
    , trace_source_(rgp_trace_source)
    , userdata_view_model_(userdata_view_model)
    , delay_timer_(delay_timer)
{
    Q_ASSERT(trace_source_ != nullptr);

    qRegisterMetaType<RgpFileValidatorStatus>();

    delay_timer_->SetSingleShot(true);
    delay_timer_->SetOnTimerFire([&] {
        is_delay_capture_in_progress_ = false;
        if (trace_source_->RequestBeginTrace(delayed_capture_connection_id_, delayed_capture_mode_) != devtrace::Result::kSuccess)
        {
            logger_->Error("Delay Timer: There was an error making the request to begin tracing.", 0, 0);
        }
    });

    trace_source_->RegisterStatusEvent({this, OnStatusEventCallback});
    trace_source_->RegisterTraceCaptureProgressEvent({this, OnTraceCaptureProgressEventCallback});
    trace_source_->RegisterSupportEvent({this, OnRgpTraceSourceSupportEvent});

    // Connect the capture target changed signal to refresh capture modes
    connect(this, &TraceSourceViewModel::CaptureTargetChanged, this, &ProfilingViewModel::OnCaptureTargetChanged);
}

ProfilingViewModel::~ProfilingViewModel() = default;

void ProfilingViewModel::RequestBeginTrace()
{
    const DDConnectionId capture_target = GetCaptureTarget();
    const uint32_t       capture_mode   = current_capture_mode_;

    if (trace_source_ == nullptr || capture_target == 0)
    {
        return;
    }

    const auto [delay_enabled, delay_ms] = userdata_view_model_->GetDelayInfo();
    if (!delay_enabled)
    {
        if (trace_source_->RequestBeginTrace(capture_target, capture_mode) != devtrace::Result::kSuccess)
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
    delayed_capture_mode_          = capture_mode;

    // Set flag to indicate delay capture is in progress
    is_delay_capture_in_progress_ = true;

    // Show progress bar with infinite indicator during delay
    emit ShowProgressUi();
    emit ProgressInfoStep(ProgressInfo{.show_progress_bar = true, .can_cancel = true, .progress = 0.0F, .progress_text = "Waiting for delayed capture..."});

    delay_timer_->SetInterval(static_cast<int>(delay_ms));
    delay_timer_->Start();
}

std::vector<CaptureMode> ProfilingViewModel::GetAvailableCaptureModes()
{
    return available_capture_modes_;
}

uint32_t ProfilingViewModel::GetCaptureMode() const
{
    return current_capture_mode_;
}

void ProfilingViewModel::SetCaptureMode(const uint32_t mode)
{
    current_capture_mode_ = mode;
}

void ProfilingViewModel::OnCaptureTargetChanged(uint16_t capture_target)
{
    // Refresh the available capture modes for the new capture target
    if (trace_source_ == nullptr)
    {
        return;
    }

    const devtrace::TraceSourceStatus& status = GetSourceStatus();
    if (!status.current_connections.contains(capture_target))
    {
        return;
    }

    std::vector<uint32_t> raw_capture_modes;
    trace_source_->GetSupportedCaptureModes(capture_target, raw_capture_modes);

    std::vector<CaptureMode> capture_modes;
    bool                     current_capture_mode_available = false;

    for (const uint32_t raw_mode : raw_capture_modes)
    {
        // The automatic capture mode should always be present since it should always be supported. However, if there are additional explicit capture modes,
        // the automatic mode will just get mapped to those, so we don't need to show it in the options list.
        if (raw_mode == 0 && raw_capture_modes.size() > 1)
        {
            continue;
        }

        capture_modes.emplace_back(CaptureMode{TextForCaptureMode(raw_mode), raw_mode});
        current_capture_mode_available |= raw_mode == current_capture_mode_;
    }

    // If the current capture modes differ from the available ones, refresh them
    if (available_capture_modes_ != capture_modes)
    {
        available_capture_modes_ = capture_modes;
        emit AvailableCaptureModesChanged(available_capture_modes_);
    }

    // If the current capture mode is not available, change it to a default one
    if (!current_capture_mode_available)
    {
        const auto api        = status.current_connections.at(capture_target);
        current_capture_mode_ = GetDefaultCaptureMode(capture_modes, status.application_name, api);
        emit CurrentCaptureModeChanged(current_capture_mode_);
    }
}

void ProfilingViewModel::Update() const
{
    trace_source_->QueryStatus();
}

void ProfilingViewModel::RequestAbort()
{
    delay_timer_->Stop();
    is_delay_capture_in_progress_ = false;

    TraceSourceViewModel::RequestAbort();
}

void ProfilingViewModel::RequestAbort(DDConnectionId umd_connection_id)
{
    delay_timer_->Stop();
    is_delay_capture_in_progress_ = false;

    TraceSourceViewModel::RequestAbort(umd_connection_id);
}

QString ProfilingViewModel::GetProcessingText()
{
    return kTraceProgressSpmCounter;
}

QString ProfilingViewModel::GetToolApplicationPath() const
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("rgp_path"))
    {
        return "";
    }

    return tool_settings_->value("rgp_path").toString();
}

void ProfilingViewModel::TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, bool file_deleted)
{
    bool deleted = file_deleted;
    if (status == devtrace::TraceCompletionStatus::kCompleted)
    {
        // Validate the trace file for profile data integrity and required content.
        const RgpFileValidatorStatus validation_status = RgpValidateProfileData(path.c_str());
        if (validation_status != kRgpFileValidatorStatusOk)
        {
            emit InvalidTraceCounterData(path.c_str(), validation_status);
            deleted = true;
        }
    }

    TraceSourceViewModel::TraceCompleted(status, path, deleted);
}

uint32_t ProfilingViewModel::GetDefaultCaptureMode(const std::vector<CaptureMode>& available_modes, const std::string& app_name, const devtrace::Api api) const
{
    devtrace::DefaultCaptureMode mode;
    mode.application = app_name;
    mode.api         = api;

    if (!default_capture_modes_.contains(mode))
    {
        Q_ASSERT(!available_modes.empty());
        return available_modes.front().mode;
    }

    const uint32_t capture_mode = default_capture_modes_.find(mode)->capture_mode;

    if (const auto find_result = std::ranges::find_if(available_modes, [&](const CaptureMode& m) { return m.mode == capture_mode; });
        find_result == available_modes.end())
    {
        Q_ASSERT(!available_modes.empty());
        return available_modes.front().mode;
    }

    if (const auto auto_capture_mode = userdata_view_model_->GetAutoCaptureMode(); auto_capture_mode == devtrace::AutoCaptureMode::kAutoCaptureModeFrameIndex)
    {
        mode.capture_mode = static_cast<uint32_t>(devtrace::RgpCaptureMode::kFrame);
    }
    else if (auto_capture_mode != devtrace::AutoCaptureMode::kAutoCaptureModeNone)
    {
        mode.capture_mode = static_cast<uint32_t>(devtrace::RgpCaptureMode::kDispatch);
    }

    return capture_mode;
}

void ProfilingViewModel::UpdateDefaultCaptureMode() const
{
    devtrace::TraceSourceStatus status = GetSourceStatus();
    if (status.application_name.empty())
    {
        return;
    }

    const uint32_t capture_target = GetCaptureTarget();
    if (!status.current_connections.contains(capture_target))
    {
        return;
    }

    devtrace::DefaultCaptureMode mode;
    mode.application  = status.application_name;
    mode.api          = status.current_connections[capture_target];
    mode.capture_mode = current_capture_mode_;

    userdata_view_model_->UpdateDefaultCaptureMode(mode);
}
