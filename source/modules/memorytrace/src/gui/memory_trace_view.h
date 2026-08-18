// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory tracing module view definition

#ifndef RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_VIEW_H_
#define RDP_SOURCE_MODULES_MEMORYTRACE_SRC_GUI_MEMORY_TRACE_VIEW_H_

#include <memory>

#include "common/inc/model/current_connection_model.h"
#include "common/inc/view/split_client_view.h"
#include "memory_trace_userdata_view_model.h"
#include "memory_trace_view_model.h"

namespace Ui
{
    class MemoryTraceCapture;
}

/// @brief Memory trace client view.
class MemoryTraceView final : public SplitClientFileView
{
public:
    /// @brief Constructor.
    /// @param [in] utility_view The utility view that manages settings.
    /// @param [in] view_model The view model for this view.
    /// @param [in] parent The parent widget for this widget.
    explicit MemoryTraceView(QWidget* utility_view, const std::shared_ptr<MemoryTraceViewModel>& view_model, QWidget* parent = nullptr);

    ~MemoryTraceView() override;

private Q_SLOTS:
    /// @brief Handles response to current connections changing.
    /// @param [in] connections The current connections.
    void OnCurrentConnectionsChanged(const std::unordered_map<DDConnectionId, devtrace::Api>& connections) const;

    void OnEnableCaptureUi() const;

    void OnEnableProgressUi() const;

    void OnTraceFailed();

    void OnTraceAborted();

    void OnTraceEnded();

    /// @brief Handles the case where the application executable is missing.
    /// @param [in] path The path to the missing executable.
    void OnApplicationExecutableMissing(const QString& path) const;

    void OnUiStatusChanged(bool should_enable) const;

    void OnInsertMarker();

private:
    void SetupConnections();

    ModelBinder                                 model_binder_;              ///< Utility object used to bind to a model.
    std::unique_ptr<Ui::MemoryTraceCapture>     capture_ui_;                ///< The capture UI for this view.
    std::weak_ptr<MemoryTraceViewModel>         view_model_;                ///< The view model.
    std::weak_ptr<MemoryTraceUserdataViewModel> userdata_view_model_;       ///< The user data view model.
    CurrentConnectionModel*                     current_connection_model_;  ///< The model that manages the current connections.
};

#endif
