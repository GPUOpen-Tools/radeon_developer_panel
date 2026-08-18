// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration of an individual device clock mode widget object.

#ifndef RDP_MODULES_DEVICE_CLOCKS_CLOCK_MODE_WIDGET_H_
#define RDP_MODULES_DEVICE_CLOCKS_CLOCK_MODE_WIDGET_H_

#include <memory>
#include <vector>

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QWidget>

#include <common/inc/view/model_binder.h>

#include "clock_mode_model.h"

// ReSharper disable once CppInconsistentNaming
namespace Ui
{
    class ClockModeWidget;
    class ClockModeOption;
}  // namespace Ui

/// @brief A frame that will emit a signal every time it receives a left mouse press, consuming the mouse event.
class ClickableFrame final : public QFrame
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit ClickableFrame(QWidget* parent = nullptr);

signals:
    /// @brief Emitted when the left mouse button is pressed on this widget.
    void Pressed();

private:
    /// @brief Handles mouse press events, calling Pressed() as needed.
    /// @param [in] event The mouse event.
    void mousePressEvent(QMouseEvent* event) override;
};

/// A widget used to display clock frequencies.
class ClockModeWidget final : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param parent The parent widget.
    explicit ClockModeWidget(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~ClockModeWidget() override;

    /// @brief Sets the model backing this widget.
    /// @param [in] model The model that this widget should use.
    void SetModel(const std::shared_ptr<class DeviceClocksGpuModel>& model);

private:
    /// @brief Removes all of the existing options.
    void RemoveOptions();

    /// @brief Adds an option for the mode.
    /// @param [in] model The mode model.
    /// @param [in] index The index of the new option.
    void AddOption(const std::shared_ptr<ClockModeModel>& model, int index);

private slots:

    /// @brief Requests the clock mode at the given index.
    /// @param [in] index The index of the clock mode to request.
    void RequestClockMode(int index) const;

    /// @brief Respond to clock mode change
    /// @param [in] index The index of the new clock mode set
    /// @note This will only be called on a successful clock mode change request
    void OnClockModeChanged(int index) const;

private:
    std::unique_ptr<Ui::ClockModeWidget>              ui_;            ///< Qt ui
    std::shared_ptr<DeviceClocksGpuModel>             model_;         ///< clock model
    std::vector<std::unique_ptr<Ui::ClockModeOption>> option_uis_;    ///< UI for all of the options.
    std::vector<QWidget*>                             options_;       ///< Options widgets.
    QButtonGroup                                      button_group_;  ///< Button group for all of the radio buttons.
    ModelBinder                                       model_binder_;  ///< Object used to bind to the model.
};
#endif
