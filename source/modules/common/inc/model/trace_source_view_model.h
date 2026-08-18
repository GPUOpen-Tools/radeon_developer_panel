// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a base view model to manipulate a trace source.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_TRACE_SOURCE_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_TRACE_SOURCE_VIEW_MODEL_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <QColor>
#include <QMetaType>
#include <QObject>
#include <QSettings>
#include <QString>

#include <MercuryModuleExt.h>
#include <ddApi.h>
#include <ddModule.h>

#include <trace_source.h>

#include "../logger.h"
#include "file_utils.h"
#include "qt_trace_io.h"
#include "trace_file_opener.h"

#include <common/inc/model/mercury_logger.h>

using CurrentConnections = std::unordered_map<uint16_t, devtrace::Api>;
Q_DECLARE_METATYPE(CurrentConnections);

/// @brief Information about the output path for a trace view model.
struct OutputPathInfo
{
    QString expanded_path;  ///< The path expanded with all of the macros expanded.
    QString raw_path;       ///< The path without any macros expanded.
};

/// @brief The information about the status.
struct StatusInfo
{
    QString text;   ///< The status text.
    QColor  color;  ///< The status color.
};
Q_DECLARE_METATYPE(StatusInfo);

/// @brief The information about the progress bar.
struct ProgressInfo
{
    bool show_progress_bar = false;  ///< Whether or not the progress bar needs to be shown.
    bool can_cancel        = false;  ///< true if the operation that is progressing can be canceled or not.

    float   progress      = 0.0;  ///< The current progress [0, 1].
    QString progress_text = "";   ///< The text that should be displayed alongside the progress bar.
};

/// @brief A base view model that manipulates a continuous trace source.
class TraceSourceViewModel : public QObject
{
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param [in] stream_provider Object used to provide writable streams to the trace source.
    /// @param [in] source The trace source that backs this view model.
    /// @param [in] file_utils Object used to open completed trace files.
    /// @param [in] file_opener Object used to open completed trace files.
    /// @param [in] file_extension The extension for files output by the trace source.
    /// @param [in] tool_settings Tool settings.
    /// @param [in] logger The logger to use for log messages.
    explicit TraceSourceViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                                  const std::shared_ptr<devtrace::TraceSource>&    source,
                                  const std::shared_ptr<FileUtils>&                file_utils,
                                  const std::shared_ptr<TraceFileOpener>&          file_opener,
                                  QString                                          file_extension,
                                  QSettings*                                       tool_settings,
                                  const std::shared_ptr<MercuryLogger>&            logger);

    /// @brief Destructor.
    ~TraceSourceViewModel() override = default;

    static void OnStatusEventCallback(void* listener, const devtrace::TraceSourceStatusEventArgs& args);

    static void OnTraceCompletionEventCallback(void* listener, const devtrace::TraceCompletionEventArgs& args);

    static void OnTraceCaptureProgressEventCallback(void* listener, const devtrace::TraceCaptureProgressEventArgs& args);

    /// @brief Sets the name of the application that is displayed in any views bound to this model.
    void SetDisplayApplicationName(const QString& application_name);

    /// @brief Called when the userdata is received from RDP.
    /// @param [in] data The raw bytes of the data.
    virtual bool ReceiveUserData(const std::string& data) = 0;

    uint16_t GetCaptureTarget() const;

    /// @brief Sets the capture target connection ID.
    /// @param [in] capture_target The connection ID to use for capture.
    void SetCaptureTarget(uint16_t capture_target);

    /// @brief Gets the logger used by this view model.
    /// @return The logger.
    const std::shared_ptr<MercuryLogger>& GetLogger() const;

    const devtrace::TraceSourceStatus& GetSourceStatus() const;

protected:
    /// @brief Returns whether or not the UI should be enabled for the given trace source stage.
    /// @param [in] stage The stage to check if the UI should be enabled for.
    /// @return true if the UI should be enabled, false otherwise.
    virtual bool ShouldEnableUiForTraceStage(devtrace::TraceSourceStage stage) = 0;

    /// @brief Returns whether or not the progress bar should be shown for the given stage.
    /// @param [in] stage The stage to check if the progress bar should be shown for.
    /// @return true if the progress bar should be shown, false otherwise.
    virtual bool ShouldProgressBarBeShownForStage(devtrace::TraceSourceStage stage) = 0;

    /// @brief Gets the text used for processing.
    /// @return The text used for processing
    virtual QString GetProcessingText();

signals:
    void ClientStatusChanged(StatusInfo status);

    void UiStatusChanged(bool should_enable);

    void CurrentConnectionsChanged(std::unordered_map<uint16_t, devtrace::Api> connections);

    /// @brief Emitted when the capture target changes.
    /// @param [in] capture_target The new capture target connection ID.
    void CaptureTargetChanged(uint16_t capture_target);

    void ConnectedProcessTextChanged(const QString& connected_process_text);

    /// @brief Emitted when the disabled reason changes.
    /// @param [in] reason_description The description of why the trace source is disabled, or empty if enabled.
    void DisabledReasonChanged(const QString& reason_description);

    /// @brief Emitted when attempting to open a file, but the tool executable could not be found.
    /// @param [in] path The path of the missing executable.
    void ExecutableMissing(const QString& path);

    /// @brief Emitted when attempting to open a file, but the text editor executable could not be found.
    /// @param [in] path The path of the missing executable.
    void TextEditorMissing(const QString& path);

    /// @brief Emitted when a trace is complete.
    void TraceComplete();

    /// @brief Emitted when there is a failure with a trace.
    void TraceFailed();

    /// @brief Emitted when a trace is aborted.
    void TraceAborted();

    void OutputPathInfoChanged(const OutputPathInfo& info);

    void ProgressInfoStep(ProgressInfo info);

    // Backend test related signals
    /// @brief Emitted when backend test executable is missing.
    void BackendTestExecutableMissing(const QString& exe_path);

    /// @brief Emitted when backend test finishes successfully.
    void BackendTestSucceeded(const QString& file_path, const QString& output);

    /// @brief Emitted when backend test fails (non-zero exit or crash).
    void BackendTestFailed(const QString& file_path, int exit_code, const QString& output);

public slots:

    /// @brief Launch the tool executable and open the file at the provided path.
    /// @param [in] path The path of the file to open.
    void OnOpenFile(const QString& path);

    /// @brief Opens the file with the specified path in the text editor.
    /// @param [in] path The path of the file to open in the text editor.
    void OnOpenFileWithTextEditor(const QString& path);

    /// @brief Called when a file is removed by the user on disk.
    /// @param [in] path The path of the file that was removed.
    void OnRemovedFile(const QString& path);

    /// @brief Requests that the current trace be aborted using the current capture target.
    virtual void RequestAbort();

    /// @brief Requests that the current trace be aborted.
    /// @param [in] umd_connection_id The connection ID to request the abort for.
    virtual void RequestAbort(DDConnectionId umd_connection_id);

    /// @brief Slot invoked from view when user selects Background Test.
    /// @param [in] path Trace/profile file selected.
    void OnBackendTestRequested(const QString& path);

protected:
    /// @brief Launch the tool executable and open the file at the provided path.
    /// @param [in] path The path of the file to open.
    virtual void OpenFile(const QString& path);

    /// @brief Called when a file is removed by the user on disk.
    /// @param [in] path The path of the file that was removed.
    virtual void RemovedFile(const QString& path);

    /// @brief Sets the output path for this view model.
    ///
    /// This will also expand any macros inside of the path variable.
    /// @param [in] path The new output path.
    void SetOutputPath(const QString& path);

    /// @brief Gets the path to the tool used to open trace files for the module that this view model belongs to.
    /// @return The path to the tool to open trace files with.
    virtual QString GetToolApplicationPath() const = 0;

    /// @brief Gets the path to the application used to open text files.
    /// @return The path to the application to open text files with.
    virtual QString GetTextEditorApplicationPath();

    /// @brief Gets the backend test executable path based on file extension.
    /// @return Path to backend test executable or empty if not set.
    virtual QString GetBackendTestApplicationPath() const;

    /// @brief Gets backend test args based on file extension.
    /// @return Arguments to backend test executable or empty if not set.
    virtual QString GetBackendTestApplicationArgs() const;

    /// @brief Used as the callback for when a trace is done dumping.
    /// @param [in] status The status of the finished trace.
    /// @param [in] path The path on disk of the trace.
    /// @param [in] file_deleted true if the trace file has been deleted by a child class, false otherwise.
    virtual void TraceCompleted(const devtrace::TraceCompletionStatus& status, const std::string& path, bool file_deleted);

    /// @brief Returns true if traces should be automatically opened
    /// @return true if traces should be automatically opened, false otherwise.
    [[nodiscard]] virtual bool ShouldAutoOpen() const;

    ProgressInfo ProcessProgressInfoFromCaptureProgressEventArgs(const devtrace::TraceCaptureProgressEventArgs& args);

private:
    std::string userdata_node_key_;  ///< The userdata node for the module.

    const QString                             extension_ = "";   ///< The extension for trace files.
    std::shared_ptr<FileSystemStreamProvider> stream_provider_;  ///< Object used to provide writable streams to the trace source.
    std::shared_ptr<TraceFileOpener>          file_opener_;      ///< Object used to open completed trace files.

    devtrace::TraceSourceStatus current_status_{};  ///< The current status of the trace source.

protected:
    std::shared_ptr<devtrace::TraceSource> source_;                   ///< The trace source that backs this view model.
    std::shared_ptr<FileUtils>             file_utils_;               ///< Object used to open completed trace files.
    std::shared_ptr<MercuryLogger>         logger_;                   ///< Object used for debug logging.
    QSettings*                             tool_settings_ = nullptr;  ///< The tool settings.

private:
    QString                                     display_application_name_;   ///< The name of the application this view model is connected to.
    QString                                     connected_app_name_;         ///< The name of the connected application.
    QString                                     raw_output_path_;            ///< The raw output path.
    OutputPathInfo                              output_info_;                ///< The current output path.
    std::recursive_mutex                        output_path_mutex_;          ///< The mutex for updating the output path.
    uint16_t                                    capture_target_;             ///< The current client to capture.
    std::unordered_map<uint16_t, devtrace::Api> current_connections_cache_;  ///< Cache of current connections.
};
Q_DECLARE_METATYPE(OutputPathInfo);

#endif
