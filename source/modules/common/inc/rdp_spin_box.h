// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for Spin box that doesn't scroll with the scroll wheel

#ifndef RDP_SOURCE_MODULES_COMMON_INC_RDP_SPIN_BOX_H_
#define RDP_SOURCE_MODULES_COMMON_INC_RDP_SPIN_BOX_H_

#include <QSpinBox>

class RdpSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit RdpSpinBox(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

#endif
