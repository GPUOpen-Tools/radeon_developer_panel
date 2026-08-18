// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A widget that shows the controls to delay a capture.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_CAPTURE_DELAY_WIDGET_H_
#define RDP_SOURCE_MODULES_COMMON_INC_CAPTURE_DELAY_WIDGET_H_

#include <memory>

#include <QWidget>

namespace Ui
{
    class CaptureDelayWidget;
}

/// @brief A panel that allows a capture to be delayed.
class CaptureDelayWidget : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit CaptureDelayWidget(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~CaptureDelayWidget() override;

signals:

    /// @brief Emitted when the capture delay changes.
    /// @param [in] capture_delay The number of milliseconds to delay the capture by.
    void CaptureDelayChanged(int capture_delay);

    /// @brief Emitted when whether or not a delay is applied to captures changes.
    /// @param [in] should_delay Qt::Checked if captures should be delayed.
    void ShouldDelayCaptureChanged(int should_delay);

public slots:

    /// @brief Called when the capture delay changes.
    /// @param [in] capture_delay The number of milliseconds to delay the capture by.
    void OnCaptureDelayChanged(uint32_t capture_delay);

    /// @brief Called when whether or not a delay is applied to captures changes.
    /// @param [in] should_delay true if captures should be delayed, false otherwise.
    void OnShouldDelayCaptureChanged(bool should_delay);

private:
    std::unique_ptr<Ui::CaptureDelayWidget> ui_;  ///< Qt ui.
};

#endif
