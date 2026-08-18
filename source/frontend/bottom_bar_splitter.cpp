// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team.
/// @file
/// @brief  Bottom bar splitter implementation.

#include "bottom_bar_splitter.h"

static constexpr int kLegalPositionDistanceThreshold = 25;

namespace rdp
{

    BottomBarSplitterHandle::BottomBarSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
    }

    void BottomBarSplitterHandle::mousePressEvent(QMouseEvent* event)
    {
        QSplitterHandle::mousePressEvent(event);
        is_dragging = true;
        start_pos_  = event->pos();
    }

    void BottomBarSplitterHandle::mouseReleaseEvent(QMouseEvent* event)
    {
        QSplitterHandle::mouseReleaseEvent(event);

        is_dragging                     = false;
        distance_from_closest_legal_pos = 0;
    }

    void BottomBarSplitterHandle::mouseMoveEvent(QMouseEvent* event)
    {
        QSplitterHandle::mouseMoveEvent(event);

        if (!is_dragging)
        {
            return;
        }

        const int splitter_pos = parentWidget()->mapFromGlobal(event->globalPosition()).y() - start_pos_.y();
        const int distance     = closestLegalPosition(splitter_pos) - splitter_pos;

        if (abs(distance_from_closest_legal_pos) < kLegalPositionDistanceThreshold && abs(distance) >= kLegalPositionDistanceThreshold)
        {
            emit DraggedPastMinSizeInDirection(distance < 0 ? -1 : 1);
        }

        distance_from_closest_legal_pos = distance;
    }

    BottomBarSplitter::BottomBarSplitter(QWidget* parent)
        : QSplitter(parent)
    {
    }

    QSplitterHandle* BottomBarSplitter::createHandle()
    {
        BottomBarSplitterHandle* handle = new BottomBarSplitterHandle(orientation(), this);
        connect(handle, &BottomBarSplitterHandle::DraggedPastMinSizeInDirection, this, &BottomBarSplitter::DraggedPastMinSizeInDirection);

        return handle;
    }
}  // namespace rdp
