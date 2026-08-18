// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Raytracing module utility view model class implementation.

#include "raytracing_userdata_view_model.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <utility>

#include "common/inc/definitions.h"
#include "raytracing_module_definitions.h"
#include "rra_trace_source.h"

static constexpr int kDefaultShortcutSequence = Qt::CTRL | Qt::Key_F8;

#ifdef _WIN32
static const auto kDefaultShortcut = GlobalShortcut(kCaptureKeyId, kDefaultShortcutSequence, VK_F8);
#else
static const auto kDefaultShortcut = GlobalShortcut(kCaptureKeyId, kDefaultShortcutSequence, Qt::Key_F8);
#endif

bool RaytracingUserdataViewModel::ReceiveUserData([[maybe_unused]] const std::string& data)
{
    devtrace::RraUserdata userdata;
    const QString         default_output_path = Util::GetDefaultOutputPath(kRraScenesDefaultParentFolder);
    if (devtrace::RraUserdataMapper parser; parser.Parse(data.c_str(), data.size(), default_output_path.toStdString(), userdata).has_value())
    {
        if (const auto trace_source = rra_trace_source_.lock(); trace_source != nullptr)
        {
            auto& config              = trace_source->GetConfig();
            config.enable_ray_history = userdata.enable_ray_history;

            char* end;
            config.ray_history_buffer_size = strtoull(userdata.ray_history_buffer_size.c_str(), &end, 10);

            config.enable_marker_capture = userdata.enable_marker_capture;
            config.marker_begin_string   = userdata.marker_begin_string;
            config.marker_end_string     = userdata.marker_end_string;
        }
    }

    return true;
}

RaytracingUserdataViewModel::RaytracingUserdataViewModel(const std::shared_ptr<devtrace::RraUserdataMapper>& mapper,
                                                         const std::shared_ptr<devtrace::RraTraceSource>&    rra_trace_source,
                                                         const std::shared_ptr<PrelaunchSettingsHelper>&     prelaunch_helper,
                                                         const std::string&                                  output_path_parent_folder,
                                                         const std::function<void(const std::string&)>&      apply_fn)
    : BaseUserdataViewModel(mapper, prelaunch_helper, output_path_parent_folder, apply_fn)
    , rra_trace_source_(rra_trace_source)
    , enable_ray_history_(false)
{
}

bool RaytracingUserdataViewModel::IsRayHistoryEnabled() const
{
    return enable_ray_history_;
}

const GlobalShortcut& RaytracingUserdataViewModel::GetDefaultShortcut()
{
    return kDefaultShortcut;
}

DelayInfo RaytracingUserdataViewModel::GetDelayInfo() const
{
    return {.enabled = should_delay_capture_, .delay = capture_delay_};
}

void RaytracingUserdataViewModel::ValidateData(devtrace::RraUserdata& userdata)
{
    constexpr uint32_t max_buffer_index    = static_cast<uint32_t>(devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes.size()) - 1;
    userdata.ray_history_buffer_size_index = std::min<uint32_t>(std::max<uint32_t>(userdata.ray_history_buffer_size_index, 0), max_buffer_index);

    char* end;
    if (const uint64_t buffer_size = strtoull(userdata.ray_history_buffer_size.c_str(), &end, 10); buffer_size == 0)
    {
        userdata.ray_history_buffer_size =
            devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes[RayHistoryBufferSizeIndex::kRayHistoryBufferIndexDefaultBuffer];
    }
}

void RaytracingUserdataViewModel::InitializeDefaults(devtrace::RraUserdata& userdata)
{
    BaseUserdataViewModel::InitializeDefaults(userdata);
    userdata.enable_ray_history            = true;
    userdata.ray_history_buffer_size_index = RayHistoryBufferSizeIndex::kRayHistoryBufferIndexDefaultBuffer;
    userdata.ray_history_buffer_size = devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes[RayHistoryBufferSizeIndex::kRayHistoryBufferIndexDefaultBuffer];

    userdata.should_delay_capture = should_delay_capture_;
    userdata.capture_delay        = capture_delay_;

    // Initialize shortcut to default values
    userdata.shortcut_sequence   = kDefaultShortcut.sequence;
    userdata.shortcut_native_key = kDefaultShortcut.native_key;
}

void RaytracingUserdataViewModel::OnUserdataChanged(const devtrace::RraUserdata& userdata)
{
    BaseUserdataViewModel::OnUserdataChanged(userdata);

    enable_ray_history_ = userdata.enable_ray_history;

    should_delay_capture_ = userdata.should_delay_capture;
    capture_delay_        = userdata.capture_delay;

    emit EnableRayHistoryChanged(userdata.enable_ray_history);
    emit RayHistoryBufferSizeChanged(userdata.ray_history_buffer_size);
    emit RayHistoryBufferSizeIndexChanged(userdata.ray_history_buffer_size_index);
    emit CaptureShortcutChanged(GlobalShortcut(kDefaultShortcut.id, userdata.shortcut_sequence, userdata.shortcut_native_key));
    emit ShouldDelayCaptureChanged(userdata.should_delay_capture);
    emit CaptureDelayChanged(userdata.capture_delay);
    emit EnableMarkerCaptureChanged(userdata.enable_marker_capture);
    emit MarkerBeginStringChanged(QString::fromStdString(userdata.marker_begin_string));
    emit MarkerEndStringChanged(QString::fromStdString(userdata.marker_end_string));
}

void RaytracingUserdataViewModel::HandleEnableRayHistoryChanged(const Qt::CheckState state)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        enable_ray_history_         = state == Qt::Checked;
        userdata.enable_ray_history = enable_ray_history_;
        emit EnableRayHistoryChanged(userdata.enable_ray_history);
    });
}

void RaytracingUserdataViewModel::HandleRayHistoryBufferSizeChanged(const std::string& buffer_size)
{
    PerformUpdate([&](UserdataType& userdata, const devtrace::RraUserdata& cached_userdata) {
        Q_UNUSED(cached_userdata);

        userdata.ray_history_buffer_size = buffer_size;
        emit RayHistoryBufferSizeChanged(buffer_size);
    });
}

void RaytracingUserdataViewModel::HandleRayHistoryBufferSizeIndexChanged(const uint32_t index)
{
    PerformUpdate([&](UserdataType& userdata, const devtrace::RraUserdata& cached_userdata) {
        Q_UNUSED(cached_userdata);

        userdata.ray_history_buffer_size_index = index;
        emit RayHistoryBufferSizeIndexChanged(index);
    });
}

void RaytracingUserdataViewModel::HandleCaptureShortcutChanged(const GlobalShortcut& shortcut)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        userdata.shortcut_native_key = shortcut.native_key;
        userdata.shortcut_sequence   = shortcut.sequence;
        emit CaptureShortcutChanged(GlobalShortcut(kDefaultShortcut.id, userdata.shortcut_sequence, userdata.shortcut_native_key));
    });
}

void RaytracingUserdataViewModel::HandleCaptureDelayChanged(uint32_t delay_ms)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        capture_delay_         = delay_ms;
        userdata.capture_delay = delay_ms;
        emit CaptureDelayChanged(userdata.capture_delay);
    });
}

void RaytracingUserdataViewModel::HandleShouldDelayCaptureChanged(bool should_delay)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        should_delay_capture_         = should_delay;
        userdata.should_delay_capture = should_delay;
        emit ShouldDelayCaptureChanged(should_delay);
    });
}

void RaytracingUserdataViewModel::HandleEnableMarkerCaptureChanged(const Qt::CheckState state)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        userdata.enable_marker_capture = state == Qt::Checked;
        emit EnableMarkerCaptureChanged(userdata.enable_marker_capture);
    });
}

void RaytracingUserdataViewModel::HandleMarkerBeginStringChanged(const QString& marker_string)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        userdata.marker_begin_string = marker_string.toStdString();
        emit MarkerBeginStringChanged(marker_string);
    });
}

void RaytracingUserdataViewModel::HandleMarkerEndStringChanged(const QString& marker_string)
{
    PerformUpdate([&](devtrace::RraUserdata& userdata, [[maybe_unused]] const devtrace::RraUserdata& cached_userdata) {
        userdata.marker_end_string = marker_string.toStdString();
        emit MarkerEndStringChanged(marker_string);
    });
}

void RaytracingUserdataViewModel::HandleTraceSupportChanged(const devtrace::RraTraceSourceSupportEventArgs& args)
{
    emit MarkerCaptureSupportedChanged(args.is_marker_capture_supported);
}
