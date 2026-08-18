// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Linux signal handler implementation file.

#include "linux/signal_handler.h"

//--------------------------------------------------------------
/// Add a signal handler. This sets up a callback that gets
/// called when the signal is caught
/// \param handler Signal handler callback function
/// \param signum the signal ID (SIGINT, SIGHUP etc)
//--------------------------------------------------------------
void SignalHandler::AddHandler(SigHandlerFn handler, int signum)
{
    SignalData signalData = {};

    // Set up the structure to specify the new action
    signalData.new_action.sa_handler = handler;
    sigemptyset(&signalData.new_action.sa_mask);
    signalData.new_action.sa_flags = 0;

    // set up the handler. First, store the old handler info
    sigaction(signum, nullptr, &signalData.old_action);

    if (signalData.old_action.sa_handler != SIG_IGN)
    {
        sigaction(signum, &signalData.new_action, nullptr);
    }
    m_signalMap.insert(std::make_pair(signum, signalData));
}

//--------------------------------------------------------------
/// Remove all signal handlers and restore them to their default
/// handlers.
//--------------------------------------------------------------
void SignalHandler::RemoveHandlers()
{
    std::map<int, SignalData>::iterator it;
    for (it = m_signalMap.begin(); it != m_signalMap.end(); ++it)
    {
        sigaction(it->first, &(it->second.old_action), nullptr);
    }
    m_signalMap.clear();
}
