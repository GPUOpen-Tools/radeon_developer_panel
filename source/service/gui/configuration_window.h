// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Configuration window class definition

#ifndef RDP_SOURCE_SERVICE_GUI_CONFIGURATION_WINDOW_H_
#define RDP_SOURCE_SERVICE_GUI_CONFIGURATION_WINDOW_H_

#include <memory>

#include <QDialog>
#include <QTimer>

#include "settings.h"

namespace Ui
{
    class ConfigurationWindow;
}

/// @brief Defines the configuration window for service
class ConfigurationWindow Q_DECL_FINAL : public QDialog
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] settings The application settings
    /// @param [in] parent The parent widget
    explicit ConfigurationWindow(Settings* settings, QWidget* parent = Q_NULLPTR);

    /// @brief Destructor
    ~ConfigurationWindow() Q_DECL_NOEXCEPT Q_DECL_OVERRIDE;

    /// @brief Toggles UI for changing connection port
    /// @param [in] enabled True to show UI, false to hide
    void EnableChangingPort(bool enabled);

    /// @brief QWidget::eventFilter override
    /// @param [in] object The sending object
    /// @param [in] event The event
    /// @return true if filtered, false otherwise
    bool eventFilter(QObject* object, QEvent* event) Q_DECL_OVERRIDE;

signals:
    /// @brief Signals router endpoint changed
    void RouterEndpointUpdated();

public slots:
    /// @brief Handle response to router port change
    /// @param [in] port The new router port value
    void OnRouterPortUpdated(unsigned int port);

    /// @brief Handle when RDS could not be started.
    /// @param port The port that RDS was attempted to be started on.
    void OnConnectFailed(unsigned int port);

    /// @brief Handle when RDS is started successfully.
    void OnConnectSucceeded();

    /// @brief Handle when the timer to hide the port warning is done.
    void OnPortWarningTimeout();

private slots:
    /// @brief Handle response to router port text changed
    void OnRouterPortTextChanged();

    /// @brief Handle response to default router port clicked
    void OnDefaultRouterPortClicked();

private:
    std::unique_ptr<Ui::ConfigurationWindow> ui_;                             ///< Qt ui
    Settings*                                settings_;                       ///< Application settings
    QIcon*                                   window_icon_;                    ///< Window icon
    QTimer                                   port_warning_timer_;             ///< Timer used to hide the port warning.
    bool                                     connected_after_error_ = false;  ///< true if RDS reconnected after there was an error connecting.
    bool                                     port_error_timed_out_  = false;  ///< true if the timeout has been reached after the port error was shown.
};

#endif
