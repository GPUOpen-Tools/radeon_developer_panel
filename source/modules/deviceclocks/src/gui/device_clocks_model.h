// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks model definition.

#ifndef RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_DEVICE_CLOCKS_MODEL_H_
#define RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_DEVICE_CLOCKS_MODEL_H_

#include <memory>
#include <string>

#include "clock_mode_model.h"

namespace devtrace
{
    class DeviceClocksManager;
}  // namespace devtrace

class DeviceClocksModel final : public QObject
{
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param [in] manager The device clocks manager.
    explicit DeviceClocksModel(const std::shared_ptr<devtrace::DeviceClocksManager>& manager);

signals:
    /// @brief Emitted when the available GPUs change.
    /// @param [in] gpus The different GPUs that are available.
    /// @param [in] is_connected true if connected to RDS, false otherwise.
    void GpusChanged(QVector<std::shared_ptr<class DeviceClocksGpuModel>> gpus, bool is_connected);

private:
    std::shared_ptr<devtrace::DeviceClocksManager> manager_;  ///< The device clocks manager.
    QVector<std::shared_ptr<DeviceClocksGpuModel>> gpus_;     ///< The GPUs that can be used with device clocks.
};

Q_DECLARE_METATYPE(QVector<std::shared_ptr<class DeviceClocksGpuModel>>)

#endif
