// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team.
/// @file
/// @brief  Bottom bar splitter declaration.

#ifndef RDP_SOURCE_FRONTEND_BOTTOM_BAR_SPLITTER_H_
#define RDP_SOURCE_FRONTEND_BOTTOM_BAR_SPLITTER_H_

#include <QMouseEvent>
#include <QSplitter>

namespace rdp
{
    /// @brief Custom splitter handle for bottom bar splitter.
    class BottomBarSplitterHandle : public QSplitterHandle
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] orientation The orientation of the handle.
        /// @param [in] parent The parent widget.
        explicit BottomBarSplitterHandle(Qt::Orientation orientation, QSplitter* parent);

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;

    signals:
        /// @brief Emitted when the splitter is dragged past the minimum size of a widget.
        /// @param [in] direction The direction the splitter is being dragged.
        void DraggedPastMinSizeInDirection(int direction);

    private:
        bool   is_dragging = false;  ///< true if the splitter is being dragged, false otherwise.
        QPoint start_pos_{};         ///< The mouse position when dragging starts.

        int distance_from_closest_legal_pos = 0;  ///< The previous distance from the closest legal position.
    };

    /// @brief Custom splitter class to allow for collapsing and expanding the bottom bar.
    class BottomBarSplitter : public QSplitter
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit BottomBarSplitter(QWidget* parent = nullptr);

    signals:
        /// @brief Emitted when the splitter is dragged past the minimum size of a widget.
        /// @param [in] direction The direction the splitter is being dragged.
        void DraggedPastMinSizeInDirection(int direction);

    protected:
        QSplitterHandle* createHandle() override;
    };
}  // namespace rdp

#endif
