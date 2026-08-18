// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks view definition

#ifndef RDP_MODULES_DEVICE_CLOCKS_DEVICE_CLOCKS_VIEW_
#define RDP_MODULES_DEVICE_CLOCKS_DEVICE_CLOCKS_VIEW_

#include <memory>

#include <QMutex>
#include <QScrollArea>
#include <QWidget>

#include "device_clocks_model.h"

// ReSharper disable once CppInconsistentNaming
namespace Ui
{
    class DeviceClocksView;
}

class DeviceClocksScrollArea final : public QScrollArea
{
public:
    explicit DeviceClocksScrollArea(QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QSize minimumSizeHint() const override;
};

/// Clock mode adjustment widget.
class DeviceClocksView final : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param parent The parent of this widget.
    explicit DeviceClocksView(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~DeviceClocksView() override;

    /// @brief Sets the model used for this view.
    /// @param model The new model to use for this view.
    void SetModel(const std::shared_ptr<DeviceClocksModel>& model);

private slots:

    /// @brief Called when the available GPUs change.
    /// @param [in] gpus The different GPUs that are available.
    /// @param [in] is_connected true if connected to RDS, false otherwise.
    void OnGpusChanged(QVector<std::shared_ptr<DeviceClocksGpuModel>> gpus, bool is_connected);

private:
    /// @brief Removes all of the buttons from the button group.
    void RemoveButtons();

    std::unique_ptr<Ui::DeviceClocksView>           ui_;
    QVector<std::shared_ptr<class ClockModeWidget>> clock_mode_widgets_;  ///< The widgets that allow selecting individual clock modes.
    std::shared_ptr<DeviceClocksModel>              model_;               ///< The model that powers this view.
};

#endif
