// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Logger class implementation

#include "logger.h"

Logger::Logger(const MercuryLoaderInterface* interface, const QString& tag)
    : interface_({})
    , tag_(tag)
{
    Q_UNUSED(interface)
}

Logger::~Logger()
{
}

void Logger::logPush() const
{
    if (interface_.logCb.pfnLogPushCallback)
    {
        interface_.logCb.pfnLogPushCallback(interface_.logCb.pUserdata);
    }
}

void Logger::logPop() const
{
    if (interface_.logCb.pfnLogPopCallback)
    {
        interface_.logCb.pfnLogPopCallback(interface_.logCb.pUserdata);
    }
}

void Logger::logAppend(DD_LOG_LEVEL level, QString text) const
{
    if (interface_.logCb.pfnLogAppendCallback)
    {
        QString message = tag_ + text;
        interface_.logCb.pfnLogAppendCallback(interface_.logCb.pUserdata, level, message.toStdString().c_str());
    }
}

void Logger::logInfo(QString text) const
{
    logAppend(DD_LOG_LEVEL_INFO, text);
}

void Logger::logError(QString text) const
{
    logAppend(DD_LOG_LEVEL_ERROR, text);
}

void Logger::logVerbose(QString text) const
{
    logAppend(DD_LOG_LEVEL_VERBOSE, text);
}

void Logger::logWarning(QString text) const
{
    logAppend(DD_LOG_LEVEL_WARN, text);
}
