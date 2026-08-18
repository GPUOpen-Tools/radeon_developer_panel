// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Crash analysis module utility view model class implementation.

#include "crash_analysis_userdata_view_model.h"

#include <numeric>

#include "common/inc/definitions.h"

static constexpr char kPdbDeliminator = ';';

bool CrashAnalysisUserdataViewModel::ReceiveUserData([[maybe_unused]] const std::string& data)
{
    devtrace::RgdUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRgdTracesDefaultParentFolder);
    if (devtrace::RgdUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata).has_value())
    {
        if (const auto trace_source = rgd_trace_source_.lock(); trace_source != nullptr)
        {
            devtrace::RgdTraceSourceConfig& config = trace_source->GetConfig();
            config.generate_text_summary           = userdata.generate_text_summary;
            config.generate_json_summary           = userdata.generate_json_summary;
            config.collect_wave_sgprs              = userdata.collect_wave_sgprs;
            config.collect_wave_vgprs              = userdata.collect_wave_vgprs;
            config.enable_advanced_crash           = userdata.enable_advanced_crash;
            config.disable_serialize_mem_ops       = userdata.disable_serialize_mem_ops;
            config.disable_serialize_alu_ops       = userdata.disable_serialize_alu_ops;
            config.SetSummaryOptions(userdata.summary_options);
        }
    }

    return true;
}

CrashAnalysisUserdataViewModel::CrashAnalysisUserdataViewModel(const std::shared_ptr<devtrace::RgdUserdataMapper>&                           mapper,
                                                               const std::shared_ptr<devtrace::RgdTraceSource>&                              rgd_trace_source,
                                                               [[maybe_unused]] const std::shared_ptr<CrashAnalysisPrelaunchSettingsHelper>& prelaunch_helper,
                                                               const std::string&                             output_path_parent_folder,
                                                               const std::function<void(const std::string&)>& apply_fn)
    : BaseUserdataViewModel(mapper, std::make_shared<AlwaysPrelaunchSettingsHelper>(), output_path_parent_folder, apply_fn)
    , rgd_trace_source_(rgd_trace_source)
{
}

void CrashAnalysisUserdataViewModel::InitializeDefaults(devtrace::RgdUserdata& userdata)
{
    BaseUserdataViewModel::InitializeDefaults(userdata);

    userdata.generate_text_summary = true;
    userdata.generate_json_summary = false;

    userdata.summary_options.show_marker_source = false;
    userdata.summary_options.expand_markers     = false;

    userdata.enable_advanced_crash     = true;
    userdata.disable_serialize_mem_ops = false;
    userdata.disable_serialize_alu_ops = false;
    userdata.collect_wave_sgprs        = false;
    userdata.collect_wave_vgprs        = false;

    userdata.summary_options.pdb_search_paths       = {};
    userdata.summary_options.pdb_include_subfolders = false;
}

void CrashAnalysisUserdataViewModel::OnUserdataChanged(const devtrace::RgdUserdata& userdata)
{
    BaseUserdataViewModel::OnUserdataChanged(userdata);

    emit GenerateTextSummaryChanged(userdata.generate_text_summary);
    emit GenerateJsonSummaryChanged(userdata.generate_json_summary);
    emit ShowMarkerSourceChanged(userdata.summary_options.show_marker_source);
    emit ExpandMarkersChanged(userdata.summary_options.expand_markers);

    emit EnableEnhancedCrashChanged(userdata.enable_advanced_crash);
    emit DisableSerializeMemOpsChanged(userdata.disable_serialize_mem_ops);
    emit DisableSerializeAluOpsChanged(userdata.disable_serialize_alu_ops);
    emit CollectSgprsChanged(userdata.collect_wave_sgprs);
    emit CollectVgprsChanged(userdata.collect_wave_vgprs);

    emit PdbSearchPathsChanged(PathsToString(userdata.summary_options.pdb_search_paths));
    emit PdbIncludeSubfoldersChanged(userdata.summary_options.pdb_include_subfolders);
}

void CrashAnalysisUserdataViewModel::HandleGenerateTextSummaryChanged(const int generate_text_summary)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.generate_text_summary = generate_text_summary == Qt::Checked;
        emit GenerateTextSummaryChanged(userdata.generate_text_summary);
    });
}

void CrashAnalysisUserdataViewModel::HandleGenerateJsonSummaryChanged(const int generate_json_summary)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.generate_json_summary = generate_json_summary == Qt::Checked;
        emit GenerateJsonSummaryChanged(userdata.generate_json_summary);
    });
}

void CrashAnalysisUserdataViewModel::HandleShowMarkerSourceChanged(const int show_marker_source)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.summary_options.show_marker_source = show_marker_source == Qt::Checked;
        emit ShowMarkerSourceChanged(userdata.summary_options.show_marker_source);
    });
}

void CrashAnalysisUserdataViewModel::HandleExpandMarkersChanged(const int expand_markers)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.summary_options.expand_markers = expand_markers == Qt::Checked;
        emit ExpandMarkersChanged(userdata.summary_options.expand_markers);
    });
}

void CrashAnalysisUserdataViewModel::HandleEnableEnhancedCrashChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.enable_advanced_crash = state == Qt::Checked;
        emit EnableEnhancedCrashChanged(userdata.enable_advanced_crash);
    });
}

void CrashAnalysisUserdataViewModel::HandleDisableSerializeMemOpsChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.disable_serialize_mem_ops = state == Qt::Checked;
        emit DisableSerializeMemOpsChanged(userdata.disable_serialize_mem_ops);
    });
}

void CrashAnalysisUserdataViewModel::HandleDisableSerializeAluOpsChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.disable_serialize_alu_ops = state == Qt::Checked;
        emit DisableSerializeAluOpsChanged(userdata.disable_serialize_alu_ops);
    });
}

void CrashAnalysisUserdataViewModel::HandleCollectSgprsChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.collect_wave_sgprs = state == Qt::Checked;
        emit CollectSgprsChanged(userdata.collect_wave_sgprs);
    });
}

void CrashAnalysisUserdataViewModel::HandleCollectVgprsChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.collect_wave_vgprs = state == Qt::Checked;
        emit CollectVgprsChanged(userdata.collect_wave_vgprs);
    });
}

void CrashAnalysisUserdataViewModel::HandlePdbSearchPathsChanged(const QString& paths)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.summary_options.pdb_search_paths = StringToPaths(paths);
        emit PdbSearchPathsChanged(paths);
    });
}

void CrashAnalysisUserdataViewModel::HandlePdbIncludeSubfoldersChanged(const int state)
{
    PerformUpdate([&](UserdataType& userdata, [[maybe_unused]] const devtrace::RgdUserdata& cached_userdata) {
        userdata.summary_options.pdb_include_subfolders = state == Qt::Checked;
        emit PdbIncludeSubfoldersChanged(userdata.summary_options.pdb_include_subfolders);
    });
}

void CrashAnalysisUserdataViewModel::HandleApplicationConnectedChanged(const bool connected)
{
    emit ApplicationConnectedChanged(connected);
}

void CrashAnalysisUserdataViewModel::SetHardwareCrashAnalysisSupported(const bool supported)
{
    support_event_received_            = true;
    hardware_crash_analysis_supported_ = supported;
    emit HardwareCrashAnalysisSupportedChanged(supported);
}

void CrashAnalysisUserdataViewModel::SetGprCaptureSupported(const bool supported)
{
    support_event_received_ = true;
    gpr_capture_supported_  = supported;
    emit GprCaptureSupportedChanged(supported);
}

void CrashAnalysisUserdataViewModel::OnBind()
{
    BaseUserdataViewModel::OnBind();

    // Only re-emit support-capability flags if a driver support event has already arrived.
    // If the view binds before the driver connects the live signal will deliver the correct
    // state; emitting the default false values here would incorrectly disable HCA and wipe
    // any saved SGPR/VGPR preferences.
    if (support_event_received_)
    {
        emit HardwareCrashAnalysisSupportedChanged(hardware_crash_analysis_supported_);
        emit GprCaptureSupportedChanged(gpr_capture_supported_);
    }
}

QString CrashAnalysisUserdataViewModel::PathsToString(const std::vector<std::string>& paths)
{
    if (paths.empty())
    {
        return {};
    }

    return std::accumulate(std::next(paths.begin()), paths.end(), QString::fromStdString(paths.front()), [](const QString& acc, const std::string& path) {
        return acc + kPdbDeliminator + QString::fromStdString(path);
    });
}

std::vector<std::string> CrashAnalysisUserdataViewModel::StringToPaths(const QString& str)
{
    std::vector<std::string> paths;

    if (!str.isEmpty())
    {
        for (const QString& path : str.split(kPdbDeliminator))
        {
            if (!path.isEmpty())
            {
                paths.push_back(path.toStdString());
            }
        }
    }

    return paths;
}
