// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A system wide keyboard handler implementation (Win32 only)

#include <functional>

#include "global_shortcut_manager.h"
#include "system_keyboard_hook.h"

class SystemKeyboardHook::KeyboardHookImpl
{
public:
    /// @brief A callback that can be used to emit errors for registration.
    /// @param error_code The error code.
    using ErrorCallback = std::function<void(DWORD)>;

    KeyboardHookImpl() = default;

    explicit KeyboardHookImpl(ErrorCallback error_callback)
        : handle_(nullptr)
        , error_callback_(error_callback)
    {
    }

    virtual ~KeyboardHookImpl();

    bool Connect()
    {
#ifdef Q_OS_WINDOWS
#ifdef DISABLE_KEYBOARD_HANDLER_IN_DEBUGGER_WIN
        if (IsDebuggerPresent() == 1)
        {
            return true;
        }
#endif
#endif

        if (handle_ == nullptr)
        {
            handle_ = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(nullptr), 0);
            if (handle_ == nullptr)
            {
                error_callback_(GetLastError());
            }
        }

        return handle_ == nullptr;
    }

    void Disconnect()
    {
        if (handle_ != nullptr)
        {
            UnhookWindowsHookEx(handle_);
            handle_ = nullptr;
        }
    }

    bool Reconnect()
    {
        Disconnect();
        return Connect();
    }

private:
    /// @brief The function that is called when there is an error registering the hook.
    ErrorCallback error_callback_;

    /// @brief Determines if the keyboard hook is connected
    /// @return true if connected, false otherwise
    bool Connected()
    {
        return (handle_ != nullptr);
    }

    /// @brief Gets the active keyboard modifiers
    /// @return keyboard modifiers
    static int GetModifiers()
    {
        int modifiers = 0;
        if ((GetKeyState(VK_SHIFT) & 0x8000))
        {
            modifiers |= Qt::SHIFT;
        }

        if ((GetKeyState(VK_CONTROL) & 0x8000))
        {
            modifiers |= Qt::CTRL;
        }

        if ((GetKeyState(VK_MENU) & 0x8000))
        {
            modifiers |= Qt::ALT;
        }

        if ((GetKeyState(VK_LWIN & 0x8000)) || (GetKeyState(VK_RWIN & 0x8000)))
        {
            modifiers |= Qt::META;
        }

        return modifiers;
    }

    /// @brief Keyboard callback procedure (Windows specific)
    /// @param n_code determine how to process the message
    /// @param w_param The virtual-key code of the key that generated the keystroke message.
    /// @param l_param The repeat count, scan code, extended-key flag, context code, previous key-state
    /// flag, and transition-state flag.
    /// @return The result for the next keyboard hook in the change or returns 0 if the hot key
    /// has been processed.
    static LRESULT CALLBACK KeyboardProc(int n_code, WPARAM w_param, LPARAM l_param)
    {
        if (n_code == HC_ACTION)
        {
            auto* keyboard = (KBDLLHOOKSTRUCT*)l_param;
            if (w_param == WM_SYSKEYDOWN || w_param == WM_KEYDOWN)
            {
                auto shortcuts = GlobalShortcutManager::Instance().GetRegisteredShortcuts();
                for (auto& shortcut : shortcuts)
                {
                    if (shortcut.native_key == (int)keyboard->vkCode)
                    {
                        auto modifiers = GetModifiers();
                        int  mods      = 0;
                        if (shortcut.sequence & Qt::SHIFT)
                        {
                            mods |= Qt::SHIFT;
                        }

                        if (shortcut.sequence & Qt::ALT)
                        {
                            mods |= Qt::ALT;
                        }

                        if (shortcut.sequence & Qt::CTRL)
                        {
                            mods |= Qt::CTRL;
                        }

                        if (shortcut.sequence & Qt::META)
                        {
                            mods |= Qt::META;
                        }

                        if (mods == modifiers)
                        {
                            emit GlobalShortcutManager::Instance().ShortcutTriggered(shortcut);
                            return false;
                        }
                    }
                }
            }

            SystemKeyboardHook::GetInstance().Reconnect();
        }

        return CallNextHookEx(nullptr, n_code, w_param, l_param);
    }

    HHOOK handle_;  ///< Handle used by the keyboard hook
};

SystemKeyboardHook::KeyboardHookImpl::~KeyboardHookImpl() = default;

SystemKeyboardHook::SystemKeyboardHook()
{
    pimpl_ = std::make_unique<KeyboardHookImpl>([&](DWORD error_code) { emit OnRegisterFailed(error_code); });
}

SystemKeyboardHook::~SystemKeyboardHook() = default;

SystemKeyboardHook& SystemKeyboardHook::GetInstance()
{
    static SystemKeyboardHook instance;
    return instance;
}

//-----------------------------------------------------------------------------
bool SystemKeyboardHook::Connect()
{
    return pimpl_->Connect();
}

void SystemKeyboardHook::Disconnect()
{
    return pimpl_->Disconnect();
}

void SystemKeyboardHook::Reconnect()
{
    pimpl_->Reconnect();
}
