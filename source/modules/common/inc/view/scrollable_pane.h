// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for a scrollable pane that contains collapsible widgets.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_SCROLLABLE_PANE_H_
#define RDP_SOURCE_MODULES_COMMON_INC_SCROLLABLE_PANE_H_

#include <QScrollArea>
#include "model_binder.h"

/// @brief A widget that should be used as the
class ScrollablePaneContents : public QWidget
{
    Q_OBJECT
protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    /// @brief Emitted when this widget resizes.
    void Resized();
};

/// @brief A widget that will size the scroll bar around the contents so there is no horizontal scrolling.
///
/// The scroll bar will be placed in the margin for the contents.
class ScrollablePane : public QScrollArea
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit ScrollablePane(QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /// @brief Called when the contents resizes.
    void ContentsResized();

private:
    ModelBinder binder_;  ///< Object used to bind to contents.
};

#endif
