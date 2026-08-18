// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing module view model definition.

#ifndef RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_VIEW_MODEL_H_

#include <memory>

#include <rra_trace_source.h>

#include <common/inc/model/qt_trace_io.h>
#include <common/inc/model/qt_trace_timer.h>
#include <common/inc/model/trace_source_view_model.h>

#include "raytracing_userdata_view_model.h"

/// @brief Model that the raytracing view binds to.
class RaytracingViewModel final : public TraceSourceViewModel
{
    // NOLINTNEXTLINE(readability-identifier-length)
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] stream_provider Object used to provide writable streams to the trace source.
    /// @param [in] rra_trace_source The RRA trace source.
    /// @param [in] file_utils Object used to open completed trace files.
    /// @param [in] file_opener Object used to open completed trace files.
    /// @param [in] tool_settings The tool settings.
    /// @param [in] userdata_view_model The utility view model.
    /// @param [in] delay_timer The timer used to delay a capture.
    /// @param [in] logger The logger.
    explicit RaytracingViewModel(const std::shared_ptr<FileSystemStreamProvider>&    stream_provider,
                                 const std::shared_ptr<devtrace::RraTraceSource>&    rra_trace_source,
                                 const std::shared_ptr<FileUtils>&                   file_utils,
                                 const std::shared_ptr<TraceFileOpener>&             file_opener,
                                 QSettings*                                          tool_settings,
                                 const std::shared_ptr<RaytracingUserdataViewModel>& userdata_view_model,
                                 const std::shared_ptr<devtrace::TraceTimer>&        delay_timer,
                                 const std::shared_ptr<MercuryLogger>&               logger);

    /// @brief Destructor.
    ~RaytracingViewModel() override = default;

    static void OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args);

    static void OnRraTraceSupportEvent(void* object, const devtrace::RraTraceSourceSupportEventArgs& args);

    void Update() const;

public slots:  // NOLINT(*-redundant-access-specifiers)
    void RequestBeginTrace();
    void RequestAbort() override;
    void RequestAbort(DDConnectionId umd_connection_id) override;

protected:
    [[nodiscard]] bool ShouldEnableUiForTraceStage(devtrace::TraceSourceStage stage) override;
    [[nodiscard]] bool ShouldProgressBarBeShownForStage(devtrace::TraceSourceStage stage) override;

    [[nodiscard]] bool    ReceiveUserData(const std::string& data) override;
    [[nodiscard]] QString GetToolApplicationPath() const override;
    [[nodiscard]] QString GetProcessingText() override;

    void TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, bool file_deleted) override;

signals:
    /// @brief Emitted when a capture completes, and it is missing BVH data.
    void CaptureMissingBvhData();

    /// @brief Emitted when a capture completes and was missing ray history data.
    void CaptureMissingRayHistory();

    /// @brief Emitted when a capture completes and was incomplete ray history data.
    void CaptureIncompleteRayHistory();

    void ShowCaptureUi();

    void ShowProgressUi();

    void TraceSupportChanged(devtrace::RraTraceSourceSupportEventArgs args);

private:
    const std::shared_ptr<devtrace::RraTraceSource>    trace_source_;                   ///< The RRA trace source.
    const std::shared_ptr<RaytracingUserdataViewModel> userdata_view_model_;            ///< The userdata view model.
    std::shared_ptr<devtrace::TraceTimer>              delay_timer_;                    ///< The timer used to delay a capture.
    DDConnectionId                                     delayed_capture_connection_id_;  ///< The connection id of the connection to capture after the delay.
    bool                                               is_delay_capture_in_progress_ = false;  ///< true if delay capture is in progress, false otherwise.
};

#endif
