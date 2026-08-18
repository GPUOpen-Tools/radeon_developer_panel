// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks GPU model definition.

#ifndef RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_DEVICE_CLOCKS_GPU_MODEL_H_
#define RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_DEVICE_CLOCKS_GPU_MODEL_H_

#include <memory>

#include <QAbstractItemModel>
#include <QString>
#include <QThreadPool>
#include <QVector>

namespace devtrace
{
    class DeviceClocksManager;
    class DeviceClockGpu;
}  // namespace devtrace

/// @brief Device clocks GPU model.
class DeviceClocksGpuModel final : public QObject
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] manager The device clocks manager.
    /// @param [in] gpu The GPU that backs this model.
    explicit DeviceClocksGpuModel(const std::shared_ptr<devtrace::DeviceClocksManager>& manager, const std::shared_ptr<devtrace::DeviceClockGpu>& gpu);

    /// @brief Gets the name of the GPU.
    /// @return The name of the GPU.
    [[nodiscard]] QString GetGpuName() const;

    /// @brief Gets the mode models
    /// @return The mode models.
    [[nodiscard]] const QVector<std::shared_ptr<class ClockModeModel>>& GetModeModels();

signals:
    void ClockModeChanged(int index);

public:
    /// @brief Gets the current mode index.
    /// @return The current mode index.
    int GetCurrentMode();

    /// @brief Requests the mode at the given index.
    /// @param [in] index The index of the mode to set.
    bool RequestMode(int index);

private:
    std::shared_ptr<devtrace::DeviceClocksManager> manager_;  ///< The device clocks manager.
    std::shared_ptr<devtrace::DeviceClockGpu>      gpu_;      ///< The GPU that backs this model.

    QVector<std::shared_ptr<ClockModeModel>> clock_modes_;  ///< The clock modes for the GPU.
};

#endif
