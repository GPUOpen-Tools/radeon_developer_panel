// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for combo box that doesn't scroll with the scroll wheel

#ifndef RDP_MODULES_COMMON_INC_RDP_COMBO_BOX_H_
#define RDP_MODULES_COMMON_INC_RDP_COMBO_BOX_H_

#include <QComboBox>

class RdpComboBox : public QComboBox
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit RdpComboBox(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

#endif
