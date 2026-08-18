// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for collapsible pane button.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_COLLAPSIBLE_PANE_BUTTON_H_
#define RDP_SOURCE_MODULES_COMMON_INC_COLLAPSIBLE_PANE_BUTTON_H_

#include <QIcon>
#include <QPushButton>

/// @brief Button that displays as just an icon.
class CollapsiblePaneButton : public QPushButton
{
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit CollapsiblePaneButton(QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
};

#endif
