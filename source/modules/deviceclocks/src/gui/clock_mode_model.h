// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks clock model definition

#ifndef RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_CLOCK_MODE_MODEL_H_
#define RDP_SOURCE_MODULES_DEVICECLOCKS_SRC_GUI_CLOCK_MODE_MODEL_H_

#include <QAbstractItemModel>

#include <dd_clocks_api.h>

#include <device_clocks.h>

namespace system_info_utils
{
    struct GpuInfo;
}

class ClockModeModel
{
public:
    /// @brief Constructor
    /// @param [in] clock_mode The information about the clock mode.
    explicit ClockModeModel(devtrace::ClockMode clock_mode);

    /// @brief Returns the name of the mode for this model.
    /// @return The name of the mode for this model.
    [[nodiscard]] QString GetName() const;

    /// @brief Returns the description of the mode for this model.
    /// @return The description of the mode for this model.
    [[nodiscard]] QString GetDescription() const;

    /// @brief Gets the type of this clock mode.
    /// @return The type of this clock mode.
    [[nodiscard]] devtrace::ClockModeType GetType() const;

    /// @brief Gets the GPU frequency range as a string.
    /// @return The GPU frequency range as a string.
    [[nodiscard]] QString GetGpuFreqStr() const;

    /// @brief Gets the memory frequency range as a string.
    /// @return The memory frequency range as a string.
    [[nodiscard]] QString GetMemFreqStr() const;

private:
    /// @brief Gets the frequency range as a string.
    /// @param [in] min The min frequency.
    /// @param [in] max The max frequency.
    /// @return The frequency range.
    [[nodiscard]] static QString GetFreqStr(uint64_t min, uint64_t max);

    devtrace::ClockMode clock_mode_;  ///< clock mode info.
};

#endif
