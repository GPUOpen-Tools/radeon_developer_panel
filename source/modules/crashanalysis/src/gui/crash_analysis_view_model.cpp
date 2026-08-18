// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGD view model implementation.

#include "crash_analysis_view_model.h"

#include <QSettings>
#include <utility>

#include <source_userdata.h>
#include <source_userdata_mapper.h>
#include <trace_source_factory.h>

#include "common/inc/util.h"

#include "common/inc/definitions.h"
#include "crash_analysis_module_definitions.h"

bool CrashAnalysisViewModel::ReceiveUserData(const std::string& data)
{
    devtrace::RgdUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRgdTracesDefaultParentFolder);
    if (devtrace::RgdUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata).has_value())
    {
        SetOutputPath(userdata.output_path.c_str());

        if (trace_source_ != nullptr)
        {
            trace_source_->GetConfig().generate_text_summary = userdata.generate_text_summary;
            trace_source_->GetConfig().generate_json_summary = userdata.generate_json_summary;
            trace_source_->GetConfig().SetSummaryOptions(userdata.summary_options);

            trace_source_->GetConfig().enable_advanced_crash     = userdata.enable_advanced_crash;
            trace_source_->GetConfig().disable_serialize_mem_ops = userdata.disable_serialize_mem_ops;
            trace_source_->GetConfig().disable_serialize_alu_ops = userdata.disable_serialize_alu_ops;
            trace_source_->GetConfig().collect_wave_sgprs        = userdata.collect_wave_sgprs;
            trace_source_->GetConfig().collect_wave_vgprs        = userdata.collect_wave_vgprs;
        }

        return true;
    }

    logger_->Error("Failed to parse userdata", 0, 0);
    SetOutputPath(default_output_path);

    return false;
}

CrashAnalysisViewModel::CrashAnalysisViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                                               const std::shared_ptr<devtrace::RgdTraceSource>& rgd_trace_source,
                                               const std::shared_ptr<FileUtils>&                file_utils,
                                               const std::shared_ptr<TraceFileOpener>&          file_opener,
                                               std::shared_ptr<CrashAnalysisUserdataViewModel>  userdata_view_model,
                                               QSettings*                                       tool_settings,
                                               const std::shared_ptr<MercuryLogger>&            logger)
    : TraceSourceViewModel(stream_provider, rgd_trace_source, file_utils, file_opener, kRgdFileExtension, tool_settings, logger)
    , trace_source_(rgd_trace_source)
    , utility_view_model_(std::move(userdata_view_model))
    , accessory_info_({CrashAnalysisAccessoryType::kNone, ""})
{
    Q_ASSERT(trace_source_ != nullptr);

    trace_source_->RegisterSummaryEvent({.listener = this, .callback = &CrashAnalysisViewModel::OnSummaryEventCallback});

    // Register for status events to handle client connection changes
    connect(this, &TraceSourceViewModel::ConnectedProcessTextChanged, this, [this](const QString& text) {
        if (!text.isEmpty())
        {
            SetAccessoryErrorString({CrashAnalysisAccessoryType::kNone, ""});
            emit AccessoryInfoChanged(accessory_info_);
        }
    });

    if (trace_source_ != nullptr)
    {
        // Register for no crash detected events
        trace_source_->RegisterNoCrashDetectedEvent({.listener = this, .callback = &CrashAnalysisViewModel::OnNoCrashDetectedCallback});

        // Register for hardware crash analysis support events
        trace_source_->RegisterSupportEvent({.listener = this, .callback = &CrashAnalysisViewModel::OnSupportEventCallback});
    }
}

void CrashAnalysisViewModel::OnNoCrashDetectedCallback(void* listener)
{
    auto* self = static_cast<CrashAnalysisViewModel*>(listener);

    self->SetAccessoryErrorString({CrashAnalysisAccessoryType::kNoCrashDetected, ""});
    emit self->AccessoryInfoChanged(self->accessory_info_);
}

void CrashAnalysisViewModel::OnSupportEventCallback(void* listener, const devtrace::RgdTraceSourceSupportEventArgs& args)
{
    auto* self = static_cast<CrashAnalysisViewModel*>(listener);

    // PostSupportEvent is called from a DevDriver connection thread. Marshal all Qt object mutations
    // and signal emissions onto the main thread via a queued invocation so Qt view-model members are
    // only ever read and written from the thread they live on.
    QMetaObject::invokeMethod(self, [self, args] {
        self->utility_view_model_->SetHardwareCrashAnalysisSupported(args.is_hardware_crash_analysis_supported);
        self->utility_view_model_->SetGprCaptureSupported(args.is_gpr_capture_supported);

        emit self->IsCurrentHardwareApuChanged(args.is_hardware_apu);
        emit self->HardwareCrashAnalysisSupportedChanged(args.is_hardware_crash_analysis_supported);
    });
}

void CrashAnalysisViewModel::Update() const
{
    trace_source_->QueryStatus();
}

void CrashAnalysisViewModel::OnSummaryEventCallback(void* listener, const devtrace::RgdSummaryEventArgs& args)
{
    const auto self = static_cast<CrashAnalysisViewModel*>(listener);
    if (args.result.result == devtrace::Result::kExecutableNotFound)
    {
        emit self->ExecutableMissing(self->GetToolApplicationPath());
    }

    if (args.result.result != devtrace::Result::kSuccess)
    {
        self->file_utils_->RemoveFile(args.result.path.c_str());

        std::scoped_lock lock(self->accessory_mutex_);
        self->accessory_info_ = {CrashAnalysisAccessoryType::kSummaryError, args.result.error.c_str()};
        emit self->AccessoryInfoChanged(self->accessory_info_);

        return;
    }

    if (!args.result.automatically_queued || self->ShouldAutoOpen())
    {
        self->OnOpenFileWithTextEditor(args.result.path.c_str());
    }

    std::scoped_lock lock(self->accessory_mutex_);
    self->accessory_info_ = {CrashAnalysisAccessoryType::kNone, ""};
    emit self->AccessoryInfoChanged(self->accessory_info_);
}

QString CrashAnalysisViewModel::GetProcessingText()
{
    return "Generating summaries...";
}

QString CrashAnalysisViewModel::GetAccessoryErrorString() const
{
    std::scoped_lock lock(accessory_mutex_);
    return accessory_info_.error_string;
}

void CrashAnalysisViewModel::SetAccessoryErrorString(const CrashAnalysisAccessoryInfo& accessory_info)
{
    std::scoped_lock lock(accessory_mutex_);
    accessory_info_ = accessory_info;
}

void CrashAnalysisViewModel::QueueSummaryGeneration(const QString& path, const bool generate_text, const bool generate_json)
{
    if (trace_source_ == nullptr)
    {
        return;
    }

    SetAccessoryErrorString({CrashAnalysisAccessoryType::kNone, ""});

    trace_source_->QueueSummaryGeneration(path.toStdString(), generate_text, generate_json);
}

QString CrashAnalysisViewModel::GetToolApplicationPath() const
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("rgd_path"))
    {
        return "";
    }

    return tool_settings_->value("rgd_path").toString();
}

void CrashAnalysisViewModel::OpenFile(const QString& path)
{
    const QString modified_path = Util::GetPathForFileWithDifferentExtension(path, "txt");
    if (const QFileInfo file(modified_path); !file.exists() || file.size() == 0)
    {
        QueueSummaryGeneration(path, true, false);
    }
    else
    {
        OnOpenFileWithTextEditor(modified_path);
    }
}

void CrashAnalysisViewModel::RemovedFile(const QString& path)
{
    if (const QFileInfo file_info(path); file_info.completeSuffix() != kRgdFileExtension)
    {
        return;
    }

    file_utils_->RemoveFile(Util::GetPathForFileWithDifferentExtension(path, "txt"));
    file_utils_->RemoveFile(Util::GetPathForFileWithDifferentExtension(path, "json"));
}

bool CrashAnalysisViewModel::ShouldAutoOpen() const
{
    // RGD files should never be opened with a file opener, since special command line arguments need to be passed in to generate
    // the text summary. Because of this the default auto open behavior should always be disabled.
    return false;
}

bool CrashAnalysisViewModel::ShouldEnableUiForTraceStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kCapturing;
}

bool CrashAnalysisViewModel::ShouldProgressBarBeShownForStage(const devtrace::TraceSourceStage stage)
{
    return stage == devtrace::TraceSourceStage::kDumping;
}
