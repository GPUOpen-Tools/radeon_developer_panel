// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Global shortcut manager class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_MANAGER_H_
#define RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_MANAGER_H_

#include <functional>

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QSettings>

#include "global_shortcut.h"

/// @brief Native event filter handling global shortcuts
class GlobalShortcutNativeEventFilter : public QAbstractNativeEventFilter
{
public:
    /// @brief Constructor.
    /// @param error_callback The function to call when there is an error registering the filter.
    GlobalShortcutNativeEventFilter(std::function<void(const QString&)> error_callback);

    /// @brief Destructor
    ~GlobalShortcutNativeEventFilter() Q_DECL_OVERRIDE;

    /// @brief Process native event
    /// @param event_type The event type.
    /// @param message The event payload.
    /// @param result Windows only param for LRESULT pointer.
    /// @return true if processed
    bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) Q_DECL_OVERRIDE;

private:
};

/// @brief Global shortcut manager singleton
/// A singleton class that managers the registering
/// and unregistering of global keyboard shortcuts.
class GlobalShortcutManager : public QObject
{
    Q_OBJECT
public:
    /// @brief Gets the static singleton instance
    /// @return singleton instance
    static GlobalShortcutManager& Instance();

    /// @brief Destructor
    ~GlobalShortcutManager() noexcept override;

    /// @brief Registers the specified shortcut.
    ///
    /// If as shortcut is already registered the existing shortcut is replaced.
    /// If a different shortcut is registered with the same sequence and native key, this method returns false.
    ///
    /// @param [in] shortcut The shortcut to register.
    /// @return true if the shortcut was successfully registered, false otherwise.
    bool Register(const GlobalShortcut& shortcut);

    /// @brief Attempts ot find and get a registered shortcut from id
    /// @param [in] id The id for the shortcut
    /// @param [out] shortcut The registered shortcut if found
    /// @return true on success, false otherwise
    bool GetShortcutFromId(int id, GlobalShortcut& shortcut);

    /// @brief Gets the list of registered shortcuts
    /// @return list of shortcuts
    const std::vector<GlobalShortcut>& GetRegisteredShortcuts();

signals:
    /// @brief Signals shortcut triggered
    /// @param shortcut The shortcut triggered
    void ShortcutTriggered(GlobalShortcut shortcut);

private:
    /// @brief Constructor
    GlobalShortcutManager();

    /// @brief Unregisters specified shortcut
    /// @param shortcut The shortcut to unregister
    static void UnregisterShortcut(const GlobalShortcut& shortcut);

    std::vector<GlobalShortcut> shortcuts_;  ///< List of registered shortcuts

    static const constexpr char* kShortcutGroupName = "GlobalShortcuts";
    static const constexpr char* kShortcutArrayName = "Shortcuts";
};

#endif
