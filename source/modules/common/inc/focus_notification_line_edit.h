// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for a line edit that emits signals when it is focused / unfocused.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_FOCUS_NOTIFICATION_LINE_EDIT_H_
#define RDP_SOURCE_MODULES_COMMON_INC_FOCUS_NOTIFICATION_LINE_EDIT_H_

#include <QLineEdit>

/// @brief A line edit that emits signals when it is focused / unfocused.
class FocusNotificationLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget if any.
    explicit FocusNotificationLineEdit(QWidget* parent = nullptr);

signals:

    /// @brief Emitted when the line edit gets focused.
    void FocusIn();

    /// @brief Emitted when the line edit loses focus.
    void FocusOut();

protected:
    /// @brief Called when the line edit is focused.
    /// @param [in] event The focus event.
    void focusInEvent(QFocusEvent* event) override;

    /// @brief Called when the line edit is unfocused.
    /// @param [in] event The unfocus event.
    void focusOutEvent(QFocusEvent* event) override;

public:
    /// @brief Sets the text that should be used to determine the minimum size of this widget.
    /// @param [in] minimum_text The text that should be used to determine the minimum size of this widget.
    void SetMinimumText(const QString& minimum_text);

    /// @brief Returns a minimum size hint that will display the minimum text.
    /// @return The size hint.
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    QString minimum_text_;  ///< The text that should be used to determine the minimum size of this widget.
};

#endif
