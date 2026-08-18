// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for a widget that displays a list of executables.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_EXECUTABLE_LIST_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_EXECUTABLE_LIST_WIDGET_H_

#include <memory>

#include <QAbstractItemModel>
#include <QWidget>

#include "collapsible_pane.h"

namespace Ui
{
    class ExecutableListWidget;
}

namespace rdp
{
    /// @brief Widget that displays a list of applications.
    class ExecutableListWidget : public QWidget
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit ExecutableListWidget(QWidget* parent = nullptr);

        /// @brief Destructor.
        ~ExecutableListWidget() override;

        /// @brief Sets whether or not the text description should be shown.
        /// @param [in] show true if it should be shown, false otherwise.
        void SetShowingTextDescription(bool show);

        /// @brief Sets the text description for this widget.
        /// @param [in] text_desc The text description for this widget.
        void SetTextDescription(const QString& text_desc);

        /// @brief Sets the model for this widget.
        /// @param [in] model The model to use for this widget.
        void SetBaseModel(QAbstractItemModel* model);

        /// @brief Provides a size hint that will make sure that the list view doesn't scroll.
        /// @return The size hint for this widget.
        QSize minimumSizeHint() const override;

    protected:
        std::unique_ptr<Ui::ExecutableListWidget> ui_;  ///< The ui for this widget.

    private:
        bool is_showing_text_description_ = false;
    };

}  // namespace rdp

#endif
