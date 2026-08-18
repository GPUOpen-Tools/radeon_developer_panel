// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Global shortcut manager class implementation

#include "global_shortcut_manager.h"

#include <algorithm>
#include <ranges>

#include <QDebug>
#include <QKeySequence>

GlobalShortcutNativeEventFilter::GlobalShortcutNativeEventFilter(std::function<void(const QString&)> error_callback)
{
    // On non-Windows platforms, global hotkeys are not currently supported.
    Q_UNUSED(error_callback);
}

GlobalShortcutNativeEventFilter::~GlobalShortcutNativeEventFilter() = default;

bool GlobalShortcutNativeEventFilter::nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result)
{
    Q_UNUSED(result)
    Q_UNUSED(event_type)
    Q_UNUSED(message)
    return false;
}

GlobalShortcutManager::GlobalShortcutManager() = default;

GlobalShortcutManager::~GlobalShortcutManager() noexcept = default;

GlobalShortcutManager& GlobalShortcutManager::Instance()
{
    static GlobalShortcutManager instance;

    return instance;
}

const std::vector<GlobalShortcut>& GlobalShortcutManager::GetRegisteredShortcuts()
{
    return shortcuts_;
}

bool GlobalShortcutManager::GetShortcutFromId(int id, GlobalShortcut& shortcut)
{
    auto found = std::ranges::find(shortcuts_, id, &GlobalShortcut::id);
    if (found == shortcuts_.end())
    {
        return false;
    }

    shortcut = *found;
    return true;
}

bool GlobalShortcutManager::Register(const GlobalShortcut& shortcut)
{
    for (const GlobalShortcut& other : shortcuts_)
    {
        if (other.id != shortcut.id && other.native_key == shortcut.native_key && other.sequence == shortcut.sequence)
        {
            return false;
        }
    }

    auto found = std::ranges::find(shortcuts_, shortcut);
    if (found != shortcuts_.end())
    {
        // First, unregister the existing hotkey
        UnregisterShortcut(*found);

        *found = shortcut;
    }
    else
    {
        shortcuts_.push_back(shortcut);
    }

    return true;
}

void GlobalShortcutManager::UnregisterShortcut(const GlobalShortcut& shortcut)
{
    Q_UNUSED(shortcut)
}
