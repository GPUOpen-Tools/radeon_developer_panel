// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for output path row view.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_OUTPUT_PATH_ROW_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_OUTPUT_PATH_ROW_VIEW_H_

#include <QWidget>

/// @brief Widget to be used by the split client view that has a minimum size to accommodate the line edit and the accessories
class OutputPathRowView : public QWidget
{
public:
    /// @brief Constructor.
    /// @param [in] parent Parent view.
    explicit OutputPathRowView(QWidget* parent = nullptr);

    /// @brief Provides a minimum size hint that accommodates the output path line edit and all of the accessories.
    /// @return The minimum size hint.
    [[nodiscard]] QSize minimumSizeHint() const override;
};

#endif
