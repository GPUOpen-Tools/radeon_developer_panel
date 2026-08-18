// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Logger class definition

#ifndef RDP_MODULES_COMMON_LOGGER_H_
#define RDP_MODULES_COMMON_LOGGER_H_

#include <QString>

#include <MercuryModuleExt.h>

class Logger
{
public:
    /// \brief Constructor
    /// \param mercury_interface The mercury module loader interface
    /// \param tag The prefix tag applied to log messages
    Logger(const MercuryLoaderInterface* mercury_interface, const QString& tag = "");

    /// \brief Destructor
    ~Logger();

    /// \brief Push log entry onto stack
    void logPush() const;

    /// \brief Pop log entries from stack
    void logPop() const;

    /// \brief Append message to log
    /// \param level The log level
    /// \param text The string message
    void logAppend(DD_LOG_LEVEL level, QString text) const;

    /// \brief Append message to log with info log level
    /// \param text The string message
    void logInfo(QString text) const;

    /// \brief Append message to log with error log level
    /// \param text The string message
    void logError(QString text) const;

    /// \brief Append message to log with verbose log level
    /// \param text The string message
    void logVerbose(QString text) const;

    /// \brief Append message to log with warning log level
    /// \param text The string message
    void logWarning(QString text) const;

private:
    MercuryLoaderInterface interface_;  ///< mercury loader interface
    QString                tag_;        ///< prefix tag for messages
};

#endif
