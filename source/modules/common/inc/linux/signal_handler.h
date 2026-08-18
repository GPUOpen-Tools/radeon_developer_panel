// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Linux signal handler definition file.

#ifndef _SIGNAL_HANDLER_H_
#define _SIGNAL_HANDLER_H_

#include <signal.h>
#include <map>

typedef void (*SigHandlerFn)(int);

//--------------------------------------------------------------
/// Class to encapsulate linux signal handlers. A handler can be
/// added for each signal type, and all signals can be removed
/// when the program shutsdown normally.
/// Uses the sigaction() function to capture the signal.
/// signal() could be used but sigaction is more robust.
//--------------------------------------------------------------
class SignalHandler
{
public:
    SignalHandler()
    {
    }
    ~SignalHandler()
    {
    }

    void AddHandler(SigHandlerFn handler, int signum);
    void RemoveHandlers();

private:
    /// structure to hold signal information
    struct SignalData
    {
        struct sigaction new_action;  ///< the new signal information
        struct sigaction old_action;  ///< the old signal information
    };

    std::map<int, SignalData> m_signalMap;  ///< map containing a signal id to its new/old handler
};

#endif
