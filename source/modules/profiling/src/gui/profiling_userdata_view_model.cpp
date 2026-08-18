// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module utility view model class implementation.

#include "profiling_userdata_view_model.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <algorithm>

#include "common/inc/definitions.h"
#include "profiling_module_definitions.h"
#include "profiling_prelaunch_settings_helper.h"
#include "profiling_view_model.h"

static constexpr QKeyCombination kDefaultShortcutSequence = Qt::CTRL | Qt::ALT | Qt::Key_C;

static auto kDefaultShortcut = GlobalShortcut(kCaptureKeyId, kDefaultShortcutSequence, Qt::Key_C);

bool ProfilingUserdataViewModel::ReceiveUserData(const std::string& data)
{
    devtrace::RgpUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRgpProfilesDefaultParentFolder);
    if (devtrace::RgpUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata).has_value())
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

        if (const auto trace_source = rgp_trace_source_.lock(); trace_source != nullptr)
        {
            devtrace::RgpTraceSourceConfig& config = trace_source->GetConfig();
            config.enable_spm_counters             = userdata.enable_counters;
            config.enable_legacy_capture           = userdata.enable_legacy_capture;
            config.enable_inst_tracing             = userdata.enable_inst_tracing;
            config.disable_capture_timeout         = userdata.disable_capture_timeout;
            config.dispatch_start_index            = userdata.dispatch_start_index;
            config.dispatch_count                  = userdata.dispatch_count;
            config.draw_count                      = userdata.draw_count;
            config.auto_capture_mode               = userdata.auto_capture_mode;
            config.compute_auto_capture_time_ms    = userdata.compute_auto_capture_time_ms;
            config.frame_capture_index             = userdata.frame_capture_index;
            config.enable_exec_pop_tokens          = userdata.enable_exec_pop_tokens;

            if (uint32_t sqtt_buffer_index = static_cast<uint32_t>(userdata.sqtt_buffer_size_profile);
                sqtt_buffer_index < devtrace::RgpTraceSourceConfig::kSqttProfileSizes.size())
            {
                config.sqtt_memory_limit = devtrace::RgpTraceSourceConfig::kSqttProfileSizes[sqtt_buffer_index];
            }
            else
            {
                // Use the driver default
                config.sqtt_memory_limit = 0;
            }
        }
    }

    return true;
}

ProfilingUserdataViewModel::ProfilingUserdataViewModel(const std::shared_ptr<devtrace::RgpUserdataMapper>&      mapper,
                                                       const std::shared_ptr<devtrace::RgpTraceSource>&         rgp_trace_source,
                                                       const std::shared_ptr<ProfilingPrelaunchSettingsHelper>& prelaunch_helper,
                                                       const std::string&                                       output_path_parent_folder,
                                                       const std::function<void(const std::string&)>&           apply_fn)
    : BaseUserdataViewModel(mapper, prelaunch_helper, output_path_parent_folder, apply_fn)
    , rgp_trace_source_(rgp_trace_source)
    , prelaunch_helper_(prelaunch_helper)
    , auto_capture_mode_(devtrace::AutoCaptureMode::kAutoCaptureModeNone)
    , prelaunch_settings_editable_(true)
    , shader_instrumentation_supported_universally_(true)
{
}

void ProfilingUserdataViewModel::InitializeDefaults(devtrace::RgpUserdata& userdata)
{
    BaseUserdataViewModel::InitializeDefaults(userdata);

    userdata.should_delay_capture = should_delay_capture_;
    userdata.capture_delay        = capture_delay_;

    userdata.enable_inst_tracing     = false;
    userdata.enable_counters         = true;
    userdata.enable_legacy_capture   = false;
    userdata.disable_capture_timeout = false;
    userdata.enable_exec_pop_tokens  = false;

    userdata.sqtt_buffer_size_profile = devtrace::SqttBufferSizeProfiles::kDefault;
    userdata.custom_sqtt_buffer_size  = 1;

    userdata.auto_capture_mode            = devtrace::AutoCaptureMode::kAutoCaptureModeNone;
    userdata.compute_auto_capture_time_ms = kDefaultAutoCaptureTime;

    userdata.dispatch_start_index = kDefaultDispatchStart;
    userdata.dispatch_count       = kDefaultDispatchEnd;

    userdata.frame_capture_index = kDefaultFrameCaptureIndex;

    userdata.spm_counters_path      = "";
    userdata.spm_sampling_frequency = kDefaultSpmSampleFrequency;

    // Initialize shortcut to default values
    userdata.shortcut_sequence   = kDefaultShortcut.sequence;
    userdata.shortcut_native_key = kDefaultShortcut.native_key;
}

devtrace::AutoCaptureMode ProfilingUserdataViewModel::GetAutoCaptureMode() const
{
    return auto_capture_mode_;
}

const GlobalShortcut& ProfilingUserdataViewModel::GetDefaultShortcut()
{
    return kDefaultShortcut;
}

DelayInfo ProfilingUserdataViewModel::GetDelayInfo() const
{
    return {.enabled = should_delay_capture_, .delay = capture_delay_};
}

void ProfilingUserdataViewModel::ValidateData(devtrace::RgpUserdata& userdata)
{
    BaseUserdataViewModel::ValidateData(userdata);

    // Clamp dispatch_start_index >= 1. The trace stack (UberTrace render-op controller,
    // legacy RGP client) treats 0 as "no trigger registered", which would cause dispatch
    // auto-capture to never fire. The spinbox view enforces this minimum interactively;
    // this clamp normalizes the loaded userdata so the cached value used for
    // re-serialization, emission, and trace-source config is also valid.
    if (userdata.dispatch_start_index < 1)
    {
        userdata.dispatch_start_index = 1;
    }

    if (userdata.sqtt_buffer_size_profile == devtrace::SqttBufferSizeProfiles::kCustom)
    {
        userdata.sqtt_buffer_size_profile = devtrace::SqttBufferSizeProfiles::kDefault;
    }

    userdata.enable_legacy_capture = false;
}

void ProfilingUserdataViewModel::OnUserdataChanged(const devtrace::RgpUserdata& userdata)
{
    BaseUserdataViewModel::OnUserdataChanged(userdata);

    //    disable_capture_timeout_.set_value(userdata.disable_capture_timeout);

    should_delay_capture_ = userdata.should_delay_capture;
    capture_delay_        = userdata.capture_delay;

    emit ShouldDelayCaptureChanged(userdata.should_delay_capture);
    emit CaptureDelayChanged(userdata.capture_delay);
    emit CaptureShortcutChanged(GlobalShortcut(kDefaultShortcut.id, userdata.shortcut_sequence, userdata.shortcut_native_key));

    emit InstructionTracingEnabledChanged(userdata.enable_inst_tracing);
    emit SpmCaptureEnabledChanged(userdata.enable_counters);
    emit SqttProfileIndexChanged(userdata.sqtt_buffer_size_profile);

    emit LegacyCaptureEnabledChanged(userdata.enable_legacy_capture);
    emit DisableCaptureTimeoutChanged(userdata.disable_capture_timeout);
    emit ExecPopTokensEnabledChanged(userdata.enable_exec_pop_tokens);

    // Update auto_capture_mode_ before emitting signals
    auto_capture_mode_ = userdata.auto_capture_mode;

    emit AutoCaptureModeChanged(userdata.auto_capture_mode);
    emit ComputeAutoCaptureTimeChanged(userdata.compute_auto_capture_time_ms);

    emit DispatchStartChanged(userdata.dispatch_start_index);
    emit DispatchCountChanged(userdata.dispatch_count);
    emit DrawCountChanged(userdata.draw_count);

    emit FrameCaptureIndexChanged(userdata.frame_capture_index);

    emit ShaderInstrumentationEnabledChanged(userdata.enable_shader_instrumentation);

    // Emit prelaunch settings state - initially editable until an app connects
    emit PrelaunchSettingsEditableChanged(prelaunch_settings_editable_);
    emit ShaderInstrumentationSupportedUniversallyChanged(shader_instrumentation_supported_universally_);
    emit EditShaderInstrumentationEnabledChanged(prelaunch_settings_editable_);
}

void ProfilingUserdataViewModel::HandleSqttProfileIndexChanged(devtrace::SqttBufferSizeProfiles profile)
{
    if (profile == devtrace::SqttBufferSizeProfiles::kCustom)
    {
        profile = devtrace::SqttBufferSizeProfiles::kDefault;
    }

    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.sqtt_buffer_size_profile = profile;
        emit SqttProfileIndexChanged(profile);
    });
}

void ProfilingUserdataViewModel::HandleShouldDelayCaptureChanged(bool should_delay)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        should_delay_capture_         = should_delay;
        userdata.should_delay_capture = should_delay;
        emit ShouldDelayCaptureChanged(should_delay);
    });
}

void ProfilingUserdataViewModel::HandleCaptureShortcutChanged(const GlobalShortcut& shortcut)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.shortcut_native_key = shortcut.native_key;
        userdata.shortcut_sequence   = shortcut.sequence;
        emit CaptureShortcutChanged(GlobalShortcut(kDefaultShortcut.id, userdata.shortcut_sequence, userdata.shortcut_native_key));
    });
}

void ProfilingUserdataViewModel::HandleCaptureDelayChanged(uint32_t delay_ms)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        capture_delay_         = delay_ms;
        userdata.capture_delay = delay_ms;
        emit CaptureDelayChanged(userdata.capture_delay);
    });
}

void ProfilingUserdataViewModel::HandleInstructionTracingEnabledChanged(bool enabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.enable_inst_tracing = enabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().enable_inst_tracing = enabled;
        }

        emit InstructionTracingEnabledChanged(enabled);
    });
}

void ProfilingUserdataViewModel::HandleLegacyCaptureEnabledChanged(bool enabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.enable_legacy_capture = enabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().enable_legacy_capture = enabled;
        }

        emit LegacyCaptureEnabledChanged(enabled);
    });
}

void ProfilingUserdataViewModel::HandleDisableCaptureTimeoutChanged(bool disabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.disable_capture_timeout = disabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().disable_capture_timeout = disabled;
        }

        emit DisableCaptureTimeoutChanged(disabled);
    });
}

void ProfilingUserdataViewModel::HandleExecPopTokensEnabledChanged(bool enabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.enable_exec_pop_tokens = enabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().enable_exec_pop_tokens = enabled;
        }

        emit ExecPopTokensEnabledChanged(enabled);
    });
}

void ProfilingUserdataViewModel::HandleSpmCaptureEnabledChanged(const bool enabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.enable_counters = enabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().enable_spm_counters = enabled;
        }

        emit SpmCaptureEnabledChanged(enabled);
    });
}

void ProfilingUserdataViewModel::HandleAutoCaptureModeChanged(devtrace::AutoCaptureMode mode)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.auto_capture_mode = mode;
        auto_capture_mode_         = mode;
        emit AutoCaptureModeChanged(auto_capture_mode_);
    });
}

void ProfilingUserdataViewModel::HandleComputeAutoCaptureTimeChanged(uint32_t time_ms)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.compute_auto_capture_time_ms = time_ms;
        emit ComputeAutoCaptureTimeChanged(time_ms);
    });
}

void ProfilingUserdataViewModel::HandleDispatchStartChanged(uint32_t dispatch_start)
{
    // Defense-in-depth: the spinbox view enforces a minimum of 1 (kDispatchIndexMinimum),
    // but a malformed saved userdata JSON (or any future code path that bypasses the
    // spinbox) could still feed 0 here. The trace stack treats 0 as "no trigger
    // registered" and the capture would never fire.
    if (dispatch_start < 1)
    {
        dispatch_start = 1;
    }
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.dispatch_start_index = dispatch_start;
        emit DispatchStartChanged(dispatch_start);
    });
}

void ProfilingUserdataViewModel::HandleDispatchCountChanged(uint32_t dispatch_count)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.dispatch_count = dispatch_count;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().dispatch_count = dispatch_count;
        }

        emit DispatchCountChanged(dispatch_count);
    });
}

void ProfilingUserdataViewModel::HandleDrawCountChanged(uint32_t draw_count)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.draw_count = draw_count;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().draw_count = draw_count;
        }

        emit DrawCountChanged(draw_count);
    });
}

void ProfilingUserdataViewModel::HandleFrameCaptureIndexChanged(uint32_t frame_index)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.frame_capture_index = frame_index;
        emit FrameCaptureIndexChanged(frame_index);
    });
}

void ProfilingUserdataViewModel::HandleShaderInstrumentationEnabledChanged(const bool enabled)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        if (prelaunch_helper_->SetShaderInstrumentationEnabled(enabled))
        {
            userdata.enable_shader_instrumentation = enabled;
            emit ShaderInstrumentationEnabledChanged(enabled);

            // In public build, exec pop tokens (execution mask tracking) is unified with
            // shader instrumentation - they are both enabled/disabled together, but only
            // if the hardware supports exec pop tokens (RDNA4 or newer)
            const bool exec_pop_enabled     = enabled && exec_pop_tokens_supported_;
            userdata.enable_exec_pop_tokens = exec_pop_enabled;

            if (const auto trace_source = rgp_trace_source_.lock())
            {
                trace_source->GetConfig().enable_exec_pop_tokens = exec_pop_enabled;
            }

            emit ExecPopTokensEnabledChanged(exec_pop_enabled);
        }
        else
        {
            emit ShaderInstrumentationFailedToSet();
            emit ShaderInstrumentationEnabledChanged(false);
        }
    });
}

void ProfilingUserdataViewModel::HandleTraceSupportChanged(const devtrace::RgpTraceSourceSupportEventArgs& args)
{
    const bool universally_supported = args.is_shader_instrumentation_supported_universally;

    if (shader_instrumentation_supported_universally_ != universally_supported)
    {
        shader_instrumentation_supported_universally_ = universally_supported;
        emit ShaderInstrumentationSupportedUniversallyChanged(universally_supported);
    }

    // Handle exec/pop tokens hardware support
    const bool exec_pop_supported = args.is_exec_pop_tokens_supported;
    if (exec_pop_tokens_supported_ != exec_pop_supported)
    {
        exec_pop_tokens_supported_ = exec_pop_supported;
        emit ExecPopTokensSupportedChanged(exec_pop_supported);
    }

    // Handle SPM capture hardware support
    const bool spm_supported = args.is_spm_capture_supported;
    if (spm_capture_supported_ != spm_supported)
    {
        spm_capture_supported_ = spm_supported;
        emit SpmCaptureSupportedChanged(spm_supported);
    }

    // In public build, exec pop tokens are coupled with shader instrumentation.
    // Now that we know the hardware support, sync the exec pop tokens state to
    // the trace source config based on the current shader instrumentation setting.
    // This is needed because at userdata load time, exec_pop_tokens_supported_ is
    // not yet known, so the coupling in HandleShaderInstrumentationEnabledChanged
    // has not had a chance to run.
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        const bool exec_pop_enabled     = userdata.enable_shader_instrumentation && exec_pop_tokens_supported_;
        userdata.enable_exec_pop_tokens = exec_pop_enabled;

        if (const auto trace_source = rgp_trace_source_.lock())
        {
            trace_source->GetConfig().enable_exec_pop_tokens = exec_pop_enabled;
        }

        emit ExecPopTokensEnabledChanged(exec_pop_enabled);
    });

    // Edit shader instrumentation is enabled when prelaunch settings are editable
    emit EditShaderInstrumentationEnabledChanged(prelaunch_settings_editable_);
}

void ProfilingUserdataViewModel::HandlePrelaunchSettingsEditableChanged(bool editable)
{
    prelaunch_settings_editable_ = editable;
    emit PrelaunchSettingsEditableChanged(editable);
    emit EditShaderInstrumentationEnabledChanged(editable);

    // Auto capture settings are editable when:
    // - prelaunch settings are editable (no app connected), OR
    // - auto-capture mode is None (even if app is connected)
    const bool is_editable = prelaunch_settings_editable_ || (auto_capture_mode_ == devtrace::AutoCaptureMode::kAutoCaptureModeNone);
    emit       AutoCaptureSettingsEditableChanged(is_editable);
}

//state_subject<bool>& ProfilingUtilityViewModel::GetIsCaptureTimeoutDisabled()
//{
//    return disable_capture_timeout_;
//}

void ProfilingUserdataViewModel::UpdateDefaultCaptureMode(const devtrace::DefaultCaptureMode& capture_mode)
{
    PerformUpdate([&](devtrace::RgpUserdata& userdata, [[maybe_unused]] const devtrace::RgpUserdata& cached_userdata) {
        userdata.default_capture_modes.erase(capture_mode);
        userdata.default_capture_modes.insert(capture_mode);
    });
}
