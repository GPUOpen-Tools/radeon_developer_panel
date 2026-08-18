// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Custom shortcut configuration line edit class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_EDIT_H_
#define RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_EDIT_H_

#include <QLineEdit>

#include "global_shortcut.h"

/// @brief Custom shortcut editor widget
class GlobalShortcutEdit final : public QLineEdit
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent widget
    explicit GlobalShortcutEdit(QWidget* parent = nullptr);

    /// @brief Destructor
    ~GlobalShortcutEdit() noexcept override;

    /// @brief Sets the default shortcut for this edit
    /// @param [in] shortcut The shortcut
    void SetDefaultShortcut(const GlobalShortcut& shortcut);

    /// @brief Sets the current shortcut represented by this edit
    /// @param [in] shortcut The shortcut
    void SetShortcut(const GlobalShortcut& shortcut);

protected:
    /// @brief Event callback
    /// @param event The event
    /// @return true if handled, false otherwise
    bool event(QEvent* event) override;

private slots:
    /// @brief Handle response to edit action triggered
    void OnEditClicked();

    /// @brief Handle response to reset action triggered
    void OnResetClicked();

    /// @brief Called when editing the shortcut is done.
    void OnEditFinished();

    /// @brief Called when a global shortcut is triggered.
    /// @param [in] shortcut The shortcut that's been triggered.
    void OnGlobalShortcutTriggered(const GlobalShortcut& shortcut);

signals:
    /// @brief Emitted when the shortcut is triggered.
    void ShortcutTriggered();

    /// @brief Emitted when the shortcut is changed.
    /// @param [in] shortcut The new shortcut.
    void ShortcutChanged(const GlobalShortcut& shortcut);

private:
    QAction*       edit_action_;       ///< Edit shortcut action
    QAction*       reset_action_;      ///< Reset shortcut action
    GlobalShortcut default_;           ///< Default shortcut
    GlobalShortcut shortcut_;          ///< Shortcut
    GlobalShortcut current_shortcut_;  ///< The current shortcut that does not include any edits.
};

#endif
