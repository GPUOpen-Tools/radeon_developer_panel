// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for JSON test utils.

#ifndef RDP_TEST_UTILS_TEST_WAIT_H_
#define RDP_TEST_UTILS_TEST_WAIT_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

/// @brief Waits for the condition to be true.
/// @param [in] milliseconds The number of milliseconds to wait for the condition to be true.
/// @param [in] condition The condition to check until it is true.
/// @return true if the condition was true before the timeout, false otherwise.
static inline bool WaitForCondition(size_t milliseconds, const std::function<bool()>& condition)
{
    auto end_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::high_resolution_clock::now() < end_time)
    {
        if (condition())
        {
            return true;
        }

        std::this_thread::yield();
    }

    return false;
}

/// @brief Waits for the condition to be true.
/// @param [in] condition The condition to check until it is true.
/// @param [in] milliseconds The number of milliseconds to wait for the condition to be true.
/// @return true if the condition was true before the timeout, false otherwise.
static inline bool WaitForCondition(const std::function<bool()>& condition, size_t milliseconds = 15000)
{
    return WaitForCondition(milliseconds, condition);
}

#endif
