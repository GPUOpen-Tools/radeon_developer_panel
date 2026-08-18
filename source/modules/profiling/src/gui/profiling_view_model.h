// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module model class definition

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_VIEW_MODEL_H_

#include <chrono>
#include <memory>
#include <vector>

#include <QFileSystemWatcher>
#include <QJsonObject>

#include <source_userdata.h>

#include <rgp_trace_source.h>

#include "rgp_file_validator.h"

#include <supported_connection_set.h>

#include <common/inc/model/mercury_logger.h>
#include <common/inc/model/qt_trace_timer.h>
#include <common/inc/model/trace_source_view_model.h>

using GpaCounterContext      = _GpaCounterContext*;      // NOLINT(*-reserved-identifier)
using GpaCounterLibFuncTable = _GpaCounterLibFuncTable;  // NOLINT(*-reserved-identifier)
using GpaDerivedCounterInfo  = _GpaDerivedCounterInfo;

struct CaptureMode
{
    QString  text;      ///< The text that describes the capture mode.
    uint32_t mode = 0;  ///< The raw capture mode.

    bool operator==(const CaptureMode& other) const
    {
        return text == other.text && mode == other.mode;
    }
};

class ProfilingViewModel final : public TraceSourceViewModel
{
    // NOLINTNEXTLINE(readability-identifier-length)
    Q_OBJECT
public:
    explicit ProfilingViewModel(const std::shared_ptr<FileSystemStreamProvider>&         stream_provider,
                                const std::shared_ptr<devtrace::RgpTraceSource>&         rgp_trace_source,
                                const std::shared_ptr<FileUtils>&                        file_utils,
                                const std::shared_ptr<TraceFileOpener>&                  file_opener,
                                QSettings*                                               tool_settings,
                                const std::shared_ptr<class ProfilingUserdataViewModel>& userdata_view_model,
                                const std::shared_ptr<devtrace::TraceTimer>&             delay_timer,
                                const std::shared_ptr<MercuryLogger>&                    logger);

    ~ProfilingViewModel() override;

    static void OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args);

    static void OnRgpTraceSourceSupportEvent(void* object, const devtrace::RgpTraceSourceSupportEventArgs& args);

    /// @brief Updates the default capture mode for the currently connected app.
    void UpdateDefaultCaptureMode() const;

    /// @brief Gets the available capture modes for the capture target.
    /// @return The available capture modes for the capture target.
    std::vector<CaptureMode> GetAvailableCaptureModes();

    void Update() const;

    uint32_t GetCaptureMode() const;

    void SetCaptureMode(uint32_t mode);

public slots:  // NOLINT(*-redundant-access-specifiers)
    void RequestBeginTrace();
    void RequestAbort() override;
    void RequestAbort(DDConnectionId umd_connection_id) override;

    /// @brief Handles when the capture target changes.
    /// @param [in] capture_target The new capture target connection ID.
    void OnCaptureTargetChanged(uint16_t capture_target);

protected:
    bool                   ShouldEnableUiForTraceStage(devtrace::TraceSourceStage stage) override;
    bool                   ShouldProgressBarBeShownForStage(devtrace::TraceSourceStage stage) override;
    void                   TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, bool file_deleted) override;
    bool                   ReceiveUserData(const std::string& data) override;
    [[nodiscard]] QString  GetToolApplicationPath() const override;
    QString                GetProcessingText() override;
    [[nodiscard]] uint32_t GetDefaultCaptureMode(const std::vector<CaptureMode>& available_modes, const std::string& app_name, devtrace::Api api) const;

signals:
    /// @brief Emitted when capture completed with a profile validation failure.
    /// @param [in] path   The path to the trace file.
    /// @param [in] status The specific validation failure (e.g. missing required chunk, SPM error, parser error).
    void InvalidTraceCounterData(QString path, RgpFileValidatorStatus status);

    /// @brief Emitted when trace source support event is processed
    /// @param [in] args The event args which contain support flags for different trace features.
    void TraceSupportChanged(devtrace::RgpTraceSourceSupportEventArgs args);

    void CurrentCaptureModeChanged(uint32_t capture_mode);

    void AvailableCaptureModesChanged(const std::vector<CaptureMode>& modes);

    void ShowCaptureUi();

    void ShowProgressUi();

private:
    const std::shared_ptr<devtrace::RgpTraceSource>   trace_source_;                   ///< RGP trace source.
    const std::shared_ptr<ProfilingUserdataViewModel> userdata_view_model_;            ///< The userdata view model.
    std::unordered_set<devtrace::DefaultCaptureMode>  default_capture_modes_;          ///< The default capture modes.
    std::shared_ptr<devtrace::TraceTimer>             delay_timer_;                    ///< The timer used to delay a capture.
    DDConnectionId                                    delayed_capture_connection_id_;  ///< The connection id of the connection to capture after the delay.
    uint32_t                                          delayed_capture_mode_;           ///< The delayed capture mode.
    std::vector<CaptureMode>                          available_capture_modes_;        ///< The current list of available capture modes.
    uint32_t                                          current_capture_mode_;           ///< The current capture mode.
    bool                                              is_delay_capture_in_progress_ = false;  ///< true if delay capture is in progress, false otherwise.

    void ProcessStatusAvailableCaptureModes(const devtrace::TraceSourceStatusEventArgs& args);
};

#endif
