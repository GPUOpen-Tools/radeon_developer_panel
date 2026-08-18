// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing module view definition.

#ifndef RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_VIEW_H_
#define RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_VIEW_H_

#include <memory>

#include "common/inc/model/current_connection_model.h"
#include "common/inc/view/split_client_view.h"

#include "raytracing_userdata_view.h"
#include "raytracing_view_model.h"

namespace Ui
{
    class RaytracingView;
}

/// @brief Raytracing client view.
class RaytracingView final : public SplitClientFileView
{
public:
    /// @brief Constructor.
    /// @param [in] userdata_view The userdata view that manages settings.
    /// @param [in] view_model The view model used by this view.
    /// @param [in] userdata_view_model The user data view model used by this view.
    /// @param [in] parent The parent widget for this widget.
    explicit RaytracingView(RaytracingUserdataView*                             userdata_view,
                            const std::shared_ptr<RaytracingViewModel>&         view_model,
                            const std::shared_ptr<RaytracingUserdataViewModel>& userdata_view_model,
                            QWidget*                                            parent = nullptr);

private slots:

    void OnEnableCaptureUi() const;

    void OnEnableProgressUi() const;

    /// @brief Called when a trace is captured, and it lacks BVH data.
    void OnCaptureMissingBvhData();

    /// @brief Called when a trace is captured, and it lacks ray history data.
    void OnCaptureMissingRayDispatchData();

    /// @brief Called when a trace is captured, and it has incomplete ray history data.
    void OnCaptureIncompleteRayDispatchData();

    /// @brief Handles response to current connections changing.
    /// @param [in] connections The current connections.
    void OnCurrentConnectionsChanged(const std::unordered_map<DDConnectionId, devtrace::Api>& connections) const;

    /// @brief Called when trying to open a file and the app executable is missing.
    /// @param [in] path The path where the app executable was expected.
    void ApplicationExecutableMissing(const QString& path);

    void OnUserDataShortcutChanged(const GlobalShortcut& shortcut) const;

    /// @brief Called when a trace fails to capture.
    void OnTraceFailed();

    /// @brief Called when a trace is aborted.
    void OnTraceAborted();

    /// @brief Called when a trace is ended (failure or success).
    void OnTraceEnded();

    /// @brief Called in response to view model Ui status enablement change
    /// @param [in] should_enable Ui should be enabled or disabled.
    void OnUiStatusChanged(bool should_enable) const;

private:
    void SetupConnections();

    void AddSpecializedCaptureOptions() const;

    ModelBinder                                model_binder_;              ///< Utility object used to bind to a model.
    std::unique_ptr<Ui::RaytracingView>        ui_;                        ///< The UI for this view.
    std::weak_ptr<RaytracingViewModel>         view_model_;                ///< The view model.
    std::weak_ptr<RaytracingUserdataViewModel> userdata_view_model_;       ///< The userdata view model.
    RaytracingUserdataView*                    userdata_view_;             ///< The userdata view.
    CurrentConnectionModel*                    current_connection_model_;  ///< The model that manages the current connections.
};

#endif
