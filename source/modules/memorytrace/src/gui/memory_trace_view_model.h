// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory tracing module view model definition

#ifndef RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_MODEL_H_
#define RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_MODEL_H_

#include <memory>

#include <rmv_trace_source.h>

#include <common/inc/model/qt_trace_io.h>
#include <common/inc/model/trace_source_view_model.h>

/// @brief Model that the memory trace view binds to.
class MemoryTraceViewModel final : public TraceSourceViewModel
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] stream_provider Object used to provide writable streams to the trace source.
    /// @param [in] rmv_trace_source The RMV trace source.
    /// @param [in] file_utils Object used to open completed trace files.
    /// @param [in] file_opener Object used to open completed trace files.
    /// @param [in] tool_settings Tool settings.
    /// @param [in] logger The logger to use for log messages.
    MemoryTraceViewModel(const std::shared_ptr<FileSystemStreamProvider>& stream_provider,
                         const std::shared_ptr<devtrace::RmvTraceSource>& rmv_trace_source,
                         const std::shared_ptr<FileUtils>&                file_utils,
                         const std::shared_ptr<TraceFileOpener>&          file_opener,
                         QSettings*                                       tool_settings,
                         const std::shared_ptr<MercuryLogger>&            logger);

    ~MemoryTraceViewModel() override;

    /// @brief Callback for status events from the trace source.
    /// @param [in] object The object the callback is associated with.
    /// @param [in] args The event arguments.
    static void OnStatusEventCallback(void* object, const devtrace::TraceSourceStatusEventArgs& args);

public Q_SLOTS:
    /// @brief Requests that a marker be added to the current trace.
    /// @param [in] marker The marker to add.
    void RequestMarker(const QString& marker) const;

    /// @brief Requests that the trace source dump the current trace.
    void RequestDump() const;

    /// @brief Updates the status of the trace source.
    void Update() const;

Q_SIGNALS:
    void ShowCaptureUi();

    void ShowProgressUi();

protected:
    /// @brief Determines whether the UI should be enabled for the given trace stage.
    /// @param [in] stage The current trace stage.
    /// @return True if the UI should be enabled, false otherwise.
    bool ShouldEnableUiForTraceStage(devtrace::TraceSourceStage stage) override;

    /// @brief Determines whether the progress bar should be shown for the given trace stage.
    /// @param [in] stage The current trace stage.
    /// @return True if the progress bar should be shown, false otherwise.
    bool ShouldProgressBarBeShownForStage(devtrace::TraceSourceStage stage) override;

    /// @brief Receives user data from the userdata model.
    /// @param [in] data The userdata string.
    /// @return True if the userdata was successfully parsed, false otherwise.
    bool ReceiveUserData(const std::string& data) override;

    /// @brief Gets the processing text to show while processing a trace.
    /// @return The processing text.
    QString GetProcessingText() override;

    /// @brief Gets the RMV executable path.
    /// @return The path to the RMV executable.
    QString GetToolApplicationPath() const override;

private:
    std::shared_ptr<devtrace::RmvTraceSource> trace_source_;  ///< The trace source that backs this view model.
};

#endif
