// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A system wide keyboard handler (Win32 only)

#ifndef RDP_SOURCE_MODULES_COMMON_INC_SYSTEM_KEYBOARD_HOOK_H_
#define RDP_SOURCE_MODULES_COMMON_INC_SYSTEM_KEYBOARD_HOOK_H_

#include <windows.h>
#include <memory>

#include <QObject>

#include "global_shortcut.h"

/// @brief Interface for system wide keyboard hook
class SystemKeyboardHook : public QObject
{
    Q_OBJECT

public:
    SystemKeyboardHook();

    ~SystemKeyboardHook();

    /// @brief Gets the singleton instance
    /// @return singleton instance
    static SystemKeyboardHook& GetInstance();

    /// @brief Connects the system keyboard hook
    /// @return true on successful connection
    bool Connect();

    /// @brief Disconnects the system keyboard hook
    void Disconnect();

    /// @brief Reconnects the system keyboard hook
    void Reconnect();

signals:

    /// @brief Called when there is an error registering the keyboard hook.
    /// @param error_code The error code from registering the hook.
    void OnRegisterFailed(DWORD error_code);

private:
    class KeyboardHookImpl;
    std::unique_ptr<KeyboardHookImpl> pimpl_;  ///< Implementation handle
};
#endif
