// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the tree view that is used for the debug log widget.

#ifndef RDP_SOURCE_FRONTEND_LOGGING_DEBUG_LOG_TREE_VIEW_H_
#define RDP_SOURCE_FRONTEND_LOGGING_DEBUG_LOG_TREE_VIEW_H_

#include <QTreeView>

namespace rdp
{
    /// @brief The tree view that is used for the debug log widget.
    class DebugLogTreeView : public QTreeView
    {
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit DebugLogTreeView(QWidget* parent = nullptr);

        void setModel(QAbstractItemModel* model) override;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        void resizeEvent(QResizeEvent* event) override;

        /// @brief Updates the column widths.
        void UpdateColumnWidths();
    };

}  // namespace rdp

#endif
