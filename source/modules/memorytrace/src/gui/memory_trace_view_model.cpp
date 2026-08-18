// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory tracing module view model definition

#include "memory_trace_view_model.h"

#include <QSettings>
#include <QStandardPaths>

#include <source_userdata.h>
#include <source_userdata_mapper.h>
#include <trace_source_factory.h>

#include "common/inc/definitions.h"
#include "common/inc/util.h"

#include "memory_trace_module_definitions.h"

bool MemoryTraceViewModel::ReceiveUserData(const std::string& data)
{
    devtrace::RmvUserdataMapper parser;
    devtrace::RmvUserdata       userdata;
    const QString               default_output_path = Util::GetDefaultOutputPath(kRmvTracesDefaultParentFolder);

    if (parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata))
    {
        SetOutputPath(userdata.output_path.c_str());
        return true;
    }

    logger_->Error("Failed to parse userdata", 0, 0);
    SetOutputPath(default_output_path);
    return false;
}

void MemoryTraceViewModel::OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args)
{
    auto* self = static_cast<MemoryTraceViewModel*>(object);
    if (const auto stage = args.new_status.GetStage(); stage == devtrace::TraceSourceStage::kDumping)
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

MemoryTraceViewModel::MemoryTraceViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                                           const std::shared_ptr<devtrace::RmvTraceSource>& rmv_trace_source,
                                           const std::shared_ptr<FileUtils>&                file_utils,
                                           const std::shared_ptr<TraceFileOpener>&          file_opener,
                                           QSettings*                                       tool_settings,
                                           const std::shared_ptr<MercuryLogger>&            logger)
    : TraceSourceViewModel(stream_provider, rmv_trace_source, file_utils, file_opener, kRmvFileExtension, tool_settings, logger)
    , trace_source_(rmv_trace_source)
{
    trace_source_->RegisterStatusEvent({.listener = this, .callback = OnStatusEventCallback});
    trace_source_->RegisterTraceCaptureProgressEvent({.listener = this, .callback = OnTraceCaptureProgressEventCallback});
}

MemoryTraceViewModel::~MemoryTraceViewModel() = default;

void MemoryTraceViewModel::RequestDump() const
{
    const uint16_t capture_target = GetCaptureTarget();
    if (trace_source_ == nullptr || capture_target == 0)
    {
        return;
    }

    if (const devtrace::Result result = trace_source_->RequestDump(capture_target); result == devtrace::Result::kSuccess)
    {
        logger_->Verbose("Successfully requested to dump a trace.", 0, 0);
    }
    else
    {
        logger_->Error("Failed to request a trace to dump.", 0, 0);
    }
}

void MemoryTraceViewModel::Update() const
{
    trace_source_->QueryStatus();
}

void MemoryTraceViewModel::RequestMarker(const QString& marker) const
{
    const uint16_t capture_target = GetCaptureTarget();
    if (trace_source_ == nullptr || capture_target == 0)
    {
        return;
    }

    const std::string utf8_string = marker.toStdString();

    if (const devtrace::Result result = trace_source_->AddMarker(capture_target, utf8_string); result == devtrace::Result::kSuccess)
    {
        logger_->Verbose("Successfully added a marker to the trace.", 0, 0);
    }
    else
    {
        logger_->Error("Failed to add a marker to the trace.", 0, 0);
    }
}

QString MemoryTraceViewModel::GetProcessingText()
{
    // No-op as RMV does not currently have any special post-processing.
    return "";
}

QString MemoryTraceViewModel::GetToolApplicationPath() const
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("rmv_path"))
    {
        return "";
    }

    return tool_settings_->value("rmv_path").toString();
}

bool MemoryTraceViewModel::ShouldEnableUiForTraceStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kCapturing;
}

bool MemoryTraceViewModel::ShouldProgressBarBeShownForStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kDumping;
}
