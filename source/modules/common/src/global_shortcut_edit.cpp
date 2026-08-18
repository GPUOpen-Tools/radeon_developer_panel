// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Custom shortcut configuration line edit class implementation

#include "global_shortcut_edit.h"

#include <QDebug>
#include <QKeyEvent>

#include <qt_common/custom_widgets/message_overlay.h>

#include "global_shortcut_manager.h"

GlobalShortcutEdit::GlobalShortcutEdit(QWidget* parent)
    : QLineEdit(parent)
{
    // Explicitly disable focus for the hotkey widget. We handle giving focus
    // to the line edit through the edit hotkey button
    setFocusPolicy(Qt::FocusPolicy::NoFocus);

    setFrame(false);

    reset_action_ = new QAction(QIcon(":/refresh.svg"), "Reset shortcut", this);
    edit_action_  = new QAction(QIcon(":/create.svg"), "Edit shortcut", this);

    connect(&GlobalShortcutManager::Instance(), &GlobalShortcutManager::ShortcutTriggered, this, &GlobalShortcutEdit::OnGlobalShortcutTriggered);

    addAction(reset_action_, TrailingPosition);
    addAction(edit_action_, TrailingPosition);
    connect(reset_action_, &QAction::triggered, this, &GlobalShortcutEdit::OnResetClicked);
    connect(edit_action_, &QAction::triggered, this, &GlobalShortcutEdit::OnEditClicked);

    connect(this, &QLineEdit::editingFinished, this, &GlobalShortcutEdit::OnEditFinished);
}

GlobalShortcutEdit::~GlobalShortcutEdit() noexcept = default;

void GlobalShortcutEdit::SetDefaultShortcut(const GlobalShortcut& shortcut)
{
    default_ = shortcut_ = current_shortcut_ = shortcut;
}

void GlobalShortcutEdit::SetShortcut(const GlobalShortcut& shortcut)
{
    if (!GlobalShortcutManager::Instance().Register(shortcut))
    {
        const QString shortcut_text = QKeySequence(shortcut.sequence).toString(QKeySequence::SequenceFormat::NativeText);
        MessageOverlay::CriticalAsync("Invalid shortcut", QString("Shortcut '%1' is already in use by another feature or application.").arg(shortcut_text));

        // Revert to the last successfully registered shortcut.
        shortcut_              = current_shortcut_;
        const QString rollback = QKeySequence(current_shortcut_.sequence).toString(QKeySequence::SequenceFormat::NativeText);
        setText(rollback);
        return;
    }

    shortcut_ = current_shortcut_ = shortcut;
    const QString text            = QKeySequence(shortcut_.sequence).toString(QKeySequence::SequenceFormat::NativeText);
    setText(text);
}

bool GlobalShortcutEdit::event(QEvent* event)
{
    static const QList kIgnoreList = {Qt::Key_Control, Qt::Key_Shift, Qt::Key_Alt, Qt::Key_Meta, Qt::Key_unknown, Qt::Key_Enter, Qt::Key_Return};
    if (event->type() == QEvent::KeyPress)
    {
        const auto* key_event = dynamic_cast<QKeyEvent*>(event);
        shortcut_.sequence    = key_event->key();

        // Check if key should be ignored
        if (const auto key = static_cast<Qt::Key>(shortcut_.sequence); kIgnoreList.contains(key))
        {
            return false;
        }

        shortcut_.native_key = key_event->nativeVirtualKey();

        const Qt::KeyboardModifiers modifiers = key_event->modifiers();
        if (modifiers & Qt::ShiftModifier)
        {
            shortcut_.sequence += Qt::SHIFT;
        }
        if (modifiers & Qt::ControlModifier)
        {
            shortcut_.sequence += Qt::CTRL;
        }
        if (modifiers & Qt::AltModifier)
        {
            shortcut_.sequence += Qt::ALT;
        }
        if (modifiers & Qt::MetaModifier)
        {
            shortcut_.sequence += Qt::META;
        }

        const QString sequence = QKeySequence(shortcut_.sequence).toString(QKeySequence::NativeText);
        setText(sequence);

        setFrame(false);
        // Done editing hotkey sequence, so we clear the focus here. This will trigger
        // an editing finished.
        clearFocus();

        return true;
    }

    if (event->type() == QEvent::Type::FocusOut)
    {
        setFrame(false);
        clearFocus();
    }

    return QLineEdit::event(event);
}

void GlobalShortcutEdit::OnEditClicked()
{
    setFrame(true);
    // On edit clicked, we will give focus to the line edit. This allows
    // the user to input their key sequence.
    setFocus();
}

void GlobalShortcutEdit::OnResetClicked()
{
    emit ShortcutChanged(default_);
}

void GlobalShortcutEdit::OnEditFinished()
{
    emit ShortcutChanged(shortcut_);
}

void GlobalShortcutEdit::OnGlobalShortcutTriggered(const GlobalShortcut& shortcut)
{
    if (shortcut.id == current_shortcut_.id)
    {
        emit ShortcutTriggered();
    }
}
