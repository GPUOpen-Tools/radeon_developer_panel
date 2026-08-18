// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for static storage of a capture delay.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_DELAY_STORE_H_
#define RDP_SOURCE_MODULES_COMMON_INC_DELAY_STORE_H_

#include <cstdint>

/// @brief Information about a capture delay.
struct DelayInfo
{
    bool     enabled = false;  ///< true if the delay should be enabled, false otherwise.
    uint32_t delay   = 0;      ///< The delay in milliseconds.
};

#endif
