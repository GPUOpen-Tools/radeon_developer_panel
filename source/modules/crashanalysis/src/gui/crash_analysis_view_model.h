// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Crash analysis module client view model class definition.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_VIEW_MODEL_H_

#include <memory>
#include <mutex>

#include <rgd_trace_source.h>

#include "common/inc/model/qt_trace_io.h"
#include "common/inc/model/trace_source_view_model.h"

#include "crash_analysis_userdata_view_model.h"

/// @brief The type of accessory that should be displayed in the view.
enum class CrashAnalysisAccessoryType
{
    kNone,             ///< No accessory.
    kNoCrashDetected,  ///< No crash was detected.
    kSummaryError      ///< An error that was encountered generating a summary.
};

/// @brief Information about the current accessory that should be displayed in the view.
struct CrashAnalysisAccessoryInfo
{
    CrashAnalysisAccessoryType type = CrashAnalysisAccessoryType::kNone;  ///< The type of accessory.
    QString                    error_string;                              ///< The error string if this accessory is a summary error.
};

/// @brief Model that the crash analysis view binds to.
class CrashAnalysisViewModel final : public TraceSourceViewModel
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] stream_provider Object used to provide writable streams to the trace source.
    /// @param [in] rgd_trace_source The RGD trace source.
    /// @param [in] file_utils Object used to open completed trace files.
    /// @param [in] file_opener Object used to open completed trace files.
    /// @param [in] userdata_view_model The crash analysis userdata view model.
    /// @param [in] tool_settings Tool settings.
    /// @param [in] logger The logger.
    CrashAnalysisViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                           const std::shared_ptr<devtrace::RgdTraceSource>& rgd_trace_source,
                           const std::shared_ptr<FileUtils>&                file_utils,
                           const std::shared_ptr<TraceFileOpener>&          file_opener,
                           std::shared_ptr<CrashAnalysisUserdataViewModel>  userdata_view_model,
                           QSettings*                                       tool_settings,
                           const std::shared_ptr<MercuryLogger>&            logger);

    /// @brief Destructor.
    ~CrashAnalysisViewModel() override = default;

    static void OnSummaryEventCallback(void* listener, const devtrace::RgdSummaryEventArgs& args);

    /// @brief Callback for when no crash is detected.
    static void OnNoCrashDetectedCallback(void* listener);

    /// @brief Callback for hardware crash analysis support events.
    static void OnSupportEventCallback(void* listener, const devtrace::RgdTraceSourceSupportEventArgs& args);

    /// @brief Queues the generation of an RGD summary.
    /// @param [in] path The path of the RGD file to generate the summary for.
    /// @param [in] generate_text true if the text summary should be generated, false otherwise.
    /// @param [in] generate_json true if the json summary should be generated, false otherwise.
    void QueueSummaryGeneration(const QString& path, bool generate_text, bool generate_json);

    /// @brief Gets the current accessory info error string.
    /// @return The current error string from accessory info.
    QString GetAccessoryErrorString() const;

    /// @brief Sets the current accessory info error string.
    /// @param [in] accessory_info The error information
    void SetAccessoryErrorString(const CrashAnalysisAccessoryInfo& accessory_info);

public slots:
    /// @brief Updates the status of the trace source.
    void Update() const;

signals:
    /// @brief Emitted when the accessory info changes.
    /// @param [in] info The new accessory info.
    void AccessoryInfoChanged(const CrashAnalysisAccessoryInfo& info);

    /// @brief Emitted when APU hardware is detected.
    /// @param [in] is_apu true if the current hardware is an APU.
    void IsCurrentHardwareApuChanged(bool is_apu);

    /// @brief Emitted when hardware crash analysis support changes.
    /// @param [in] supported true if hardware crash analysis is supported.
    void HardwareCrashAnalysisSupportedChanged(bool supported);

protected:
    /// @param [in] stage The current trace stage.
    /// @return True if the UI should be enabled, false otherwise.
    bool ShouldEnableUiForTraceStage(devtrace::TraceSourceStage stage) override;

    /// @brief Determines whether the progress bar should be shown for the given trace stage.
    /// @param [in] stage The current trace stage.
    /// @return True if the progress bar should be shown, false otherwise.
    bool ShouldProgressBarBeShownForStage(devtrace::TraceSourceStage stage) override;

    QString GetProcessingText() override;

    /// @brief Called when the userdata is received from RDP.
    /// @param [in] data The raw bytes of the data.
    bool ReceiveUserData(const std::string& data) override;

    /// @brief Gets the RGD executable path.
    /// @return The path to the RGD executable.
    QString GetToolApplicationPath() const override;

    /// @brief No-op since the RGD is only launch during tool generation.
    /// @param [in] path The path of the file to open.
    void OpenFile(const QString& path) override;

    /// @brief Called when a file is removed by the user on disk and removes any files with matching names and .txt and .json extensions.
    /// @param [in] path The path of the file that was removed.
    void RemovedFile(const QString& path) override;

    /// @brief Returns true if traces should be automatically opened
    /// @return true if traces should be automatically opened, false otherwise.
    bool ShouldAutoOpen() const override;

private:
    std::shared_ptr<devtrace::RgdTraceSource>       trace_source_;        ///< The trace source that backs this view model.
    std::shared_ptr<CrashAnalysisUserdataViewModel> utility_view_model_;  ///< The utility view model.
    mutable std::mutex                              accessory_mutex_;     ///< Mutex that guards accessory info.
    CrashAnalysisAccessoryInfo                      accessory_info_;      ///< Accessory info subject.
};

#endif
