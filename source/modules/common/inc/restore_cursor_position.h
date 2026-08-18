// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A utility class used to restore a QLineEdit's cursor position
///         after the text has been edited.

#ifndef _RESTORE_CURSOR_POSITION_H_
#define _RESTORE_CURSOR_POSITION_H_

#include <QLineEdit>

//-----------------------------------------------------------------------------
/// Implementation of the RestoreCursorPosition class.
//-----------------------------------------------------------------------------
class RestoreCursorPosition
{
public:
    //-----------------------------------------------------------------------------
    /// Constructor for RestoreCursorPosition.
    /// \param pControl The QLineEdit whose cursor position will be stored.
    //-----------------------------------------------------------------------------
    explicit RestoreCursorPosition(QLineEdit* pControl)
        : m_pLineEdit(pControl)
        , m_cursorPosition(0)
    {
        if (m_pLineEdit != nullptr)
        {
            m_cursorPosition = m_pLineEdit->cursorPosition();
        }
    }

    //-----------------------------------------------------------------------------
    /// Destructor for RestoreCursorPosition.
    //-----------------------------------------------------------------------------
    ~RestoreCursorPosition()
    {
        if (m_pLineEdit != nullptr)
        {
            m_pLineEdit->setCursorPosition(m_cursorPosition);
        }
    }

private:
    QLineEdit* m_pLineEdit;
    int        m_cursorPosition;
};

#endif
