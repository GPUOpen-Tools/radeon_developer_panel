// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Main entry point.

#ifdef _WIN32
#include <Windows.h>
#endif

#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include "common/inc/single_application_instance.h"
#ifndef Q_OS_WIN
#include "common/inc/linux/signal_handler.h"
#endif

#include "definitions.h"
#include "main_window.h"
#include "settings.h"

namespace
{
    std::unique_ptr<SingleApplicationInstance> g_application_instance;
}

#ifndef Q_OS_WIN
void SigHandler(int signal)
{
    Q_UNUSED(signal)
    if (g_application_instance != nullptr)
    {
        SingleApplicationInstance::exit();
    }
}
#endif

int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(resources);

    // We only want a single instance of RDS to run on a machine.
    g_application_instance = std::make_unique<SingleApplicationInstance>(argc, argv, kRadeonDeveloperServiceGuid, true);
    if (g_application_instance == nullptr || g_application_instance->IsAnotherInstanceRunning())
    {
        // RDS is already running, so don't allow this new instance to proceed any further.
        g_application_instance.reset();
        return -1;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        // The system tray icon wasn't available to the user, but there's no good reason to stop RDS entirely.
        // Let the user know that the tray icon isn't available, but proceed anyways.
        QMessageBox::critical(nullptr, QObject::tr("Systray"), QObject::tr("Operating in Headless Mode."));
    }

    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow main_window;

#ifdef _WIN32
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif

#ifndef Q_OS_WIN
    // install signal handlers on Linux
    SignalHandler signal_handler;
    signal_handler.AddHandler(SigHandler, SIGTERM);
    signal_handler.AddHandler(SigHandler, SIGINT);
#endif

    const int result = SingleApplicationInstance::exec();

#ifndef Q_OS_WIN
    // remove signal handlers on Linux
    signal_handler.RemoveHandlers();
#endif
    g_application_instance.reset();

    return result;
}
