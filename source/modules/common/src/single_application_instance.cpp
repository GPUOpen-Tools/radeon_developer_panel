// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  SingleApplicationInstance implementation

#include "single_application_instance.h"

#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#include "single_instance.h"

const int INSTANCE_CHECK_POLL_INTERVAL = 1000;

static const QString gs_SHARED_MEMORY_LOCKED =
    "Another instance of %1 is running under a different account (process Id is %2). Please shut down the previous instance in order to start a new instance.";
static const QString gs_SHARED_MEMORY_CRASHED =
    "A shared memory file used by %1 is locked. Please run the removeSharedMemory script to delete it and then try restarting.";

// Uncomment to display debugging message boxes
// #define _DEBUG_SINGLE_APP_INSTANCE_

//-----------------------------------------------------------------------------
/// Constructor for SingleApplicationInstance.
/// \param argc The argc parameter passed into main()
/// \param argv The argv parameter passed into main()
/// \param uniqueKey The unique key used to set up shared memory. Each
/// application using this will have a unique key, and applications of the
/// same name will have the same key
/// \param checkHeadlessInstances True enables an additional check for non-GUI based
/// applications using the SingleInstance class.  False disables the extra check.
//-----------------------------------------------------------------------------
SingleApplicationInstance::SingleApplicationInstance(int& argc, char* argv[], const QString uniqueKey, bool checkHeadlessInstances)
    : QApplication(argc, argv)
    , m_pSingleInstance(nullptr)
{
    m_uniqueKey = uniqueKey;
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
    QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::SingleApplicationInstance() - Constructor called. ").exec();

    QString msg;
    msg = "SingleApplicationInstance::SingleApplicationInstance() - setKey(" + uniqueKey + ")";
    QMessageBox(QMessageBox::Information, applicationName(), msg).exec();
#endif

    m_sharedMem.setKey(uniqueKey);

#ifdef _DEBUG_SINGLE_APP_INSTANCE_
    QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::SingleApplicationInstance() - calling QSharedMem::create()...").exec();
#endif

    // Create shared memory only if it doesn't exist (i.e. if this is the primary instance).
    if (CreateSharedMemory())
    {
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        msg = "SingleApplicationInstance::SingleApplicationInstance() - QSharedMem::create() call succeeded. Key = " + m_sharedMem.nativeKey() +
              ". Do you want to crash?";
        if (QMessageBox::Yes == QMessageBox(QMessageBox::Question, applicationName(), msg, QMessageBox::Yes | QMessageBox::No).exec())
        {
            // force a crash for testing purposes
            int x = 0;
            int y = 100 / x;
            Q_UNUSED(y);
        }
#endif
        m_sharedMem.lock();

#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::SingleApplicationInstance() - Set QSharedMem flag to false")
            .exec();
#endif
        *(bool*)m_sharedMem.data() = false;
        m_sharedMem.unlock();

        m_anotherInstanceRunning = false;
        // Check for messages of other instances.
        QTimer* timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(OnCheckForNewInstance()));

#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::SingleApplicationInstance() - Start polling timer...").exec();
#endif
        timer->start(INSTANCE_CHECK_POLL_INTERVAL);
    }
    // Shared memory already exists, attach to it.
    else if (m_sharedMem.attach())
    {
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        QMessageBox(QMessageBox::Information,
                    applicationName(),
                    "SingleApplicationInstance::SingleApplicationInstance() - QSharedMem::create() call failed.  QSharedMem::Attach() succeeded.")
            .exec();
#endif
        m_anotherInstanceRunning = true;
        NotifyAppInstanceStarted();
    }
    else if (m_sharedMem.error() == QSharedMemory::PermissionDenied)
    {
        QString warningMsg;
        // This instance of the application is unable to access the shared memory because another instance created it with elevated privileges or a different account.
        int otherProcessID = FindProcessId(applicationName(), applicationPid());
        if (otherProcessID != -1)
        {
            warningMsg = gs_SHARED_MEMORY_LOCKED.arg(applicationName()).arg(QString::number(otherProcessID));
        }
        else
        {
            warningMsg = gs_SHARED_MEMORY_CRASHED.arg(applicationName());
        }
        QMessageBox(QMessageBox::Warning, applicationName(), warningMsg).exec();
        m_anotherInstanceRunning = true;
    }

    // Additional check for non-gui instances running
    if (checkHeadlessInstances)
    {
        m_pSingleInstance = new SingleInstance(uniqueKey.toLatin1());
        if ((m_pSingleInstance != nullptr) && (m_pSingleInstance->IsProgramAlreadyRunning()))
        {
            m_anotherInstanceRunning = true;
        }
    }
}

//-----------------------------------------------------------------------------
/// Destructor for SingleApplicationInstance. Add any resource cleanup here
//-----------------------------------------------------------------------------
SingleApplicationInstance::~SingleApplicationInstance()
{
    m_sharedMem.detach();
    if (m_pSingleInstance != nullptr)
    {
        delete m_pSingleInstance;
    }
}

//-----------------------------------------------------------------------------
/// Attempts to create a shared memory file.  Retries if first attempt fails
/// (cleans up old shared memory in case of previous crash)
/// \return True if create succeeds, otherwise returns false.
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::CreateSharedMemory()
{
    bool ret = false;
    if (!(ret = m_sharedMem.create(sizeof(bool))))
    {
        // Create may have failed due to a previous instance crashing.
        // Attach and detach to clean up.  Then try creating again.
        m_sharedMem.attach();
        m_sharedMem.detach();
        ret = m_sharedMem.create(sizeof(bool));
    }

    return ret;
}

//-----------------------------------------------------------------------------
/// Determines if an instance of the application already exists.
/// \return True if an instance of the application is already running, false otherwise.
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::IsAnotherInstanceRunning()
{
    return m_anotherInstanceRunning;
}

//-----------------------------------------------------------------------------
/// Determines if an instance of an application other than this one is running.
/// \param uniqueKey The identifier of the application to check.
/// \return True if an instance of the application is already running, false otherwise.
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::IsInstanceRunning(const QString& uniqueKey) const
{
    // Only check the shared memory if the uniqueKey is different than this applicacation.
    if (m_uniqueKey.compare(uniqueKey) != 0)
    {
        // For backward compatibility, use both methods of check for duplicate instances.
        // First check using QSharedMemory then check using SingleInstance mutex.
        QSharedMemory sharedMem;
        sharedMem.setKey(uniqueKey);
        if (sharedMem.attach())
        {
            return true;
        }

        SingleInstance singleInstance(uniqueKey.toLatin1());
        if (!singleInstance.IsProgramAlreadyRunning())
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
/// Determine if this instance is the primary instance
/// \return True if this is the master instance, false otherwise.
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::IsPrimaryInstance()
{
    return !IsAnotherInstanceRunning();
}

//-----------------------------------------------------------------------------
/// Poll for app started notifications.  Emits signal if another instance is detected.
//-----------------------------------------------------------------------------
void SingleApplicationInstance::OnCheckForNewInstance()
{
    m_sharedMem.lock();
    bool* pSharedMemFlag   = (bool*)m_sharedMem.data();
    bool  InstanceDetected = *pSharedMemFlag;
    if (InstanceDetected)
    {
        *pSharedMemFlag = false;
    }
    m_sharedMem.unlock();

    if (InstanceDetected)
    {
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        QMessageBox(QMessageBox::Information,
                    applicationName(),
                    "SingleApplicationInstance::OnCheckForNewInstance() - OnCheckForNewInstance() - emit signal AppInstanceStarted.")
            .exec();
#endif
        emit AppInstanceStarted();
    }
}

//-----------------------------------------------------------------------------
/// Notifies primary instance of an application that another instance has started.
/// \return True if the message can be sent, false otherwise (i.e. this is
/// the primary instance).
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::NotifyAppInstanceStarted()
{
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
    QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::NotifyAppInstanceStarted() - Enter method.").exec();
#endif
    //Prevent primary instance from sending messages.
    if (IsPrimaryInstance())
    {
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
        QMessageBox(QMessageBox::Information,
                    applicationName(),
                    "SingleApplicationInstance::NotifyAppInstanceStarted() - SingleApplicationInstance::IsPrimaryInstance() returns false.")
            .exec();
#endif
        return false;
    }

    m_sharedMem.lock();
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
    QMessageBox(QMessageBox::Information,
                applicationName(),
                "SingleApplicationInstance::NotifyAppInstanceStarted() - Set shared memory flag to true (instance detected).")
        .exec();
#endif
    bool* pNewInstanceDetectedFlag = (bool*)m_sharedMem.data();
    *pNewInstanceDetectedFlag      = true;
    m_sharedMem.unlock();

    return true;
}

//-----------------------------------------------------------------------------
/// Reimplemented from QApplication so we can throw exceptions in slots
/// \param receiver The receiver object.
/// \param event The event object.
/// \return always return false
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::notify(QObject* receiver, QEvent* event)
{
#ifdef RDP_ALLOW_UNHANDLED_EXCEPTIONS
    // For debug builds, propagate any unhandled exceptions for debugging
    return QApplication::notify(receiver, event);
#else
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch (std::exception& e)
    {
        qCritical() << "Exception thrown:" << e.what();
    }

    return false;
#endif
}

//-----------------------------------------------------------------------------
/// Determines if a string is numeric
/// \param strValue The string to check
/// \return true if the string is a base 10 number, otherwise returns false
//-----------------------------------------------------------------------------
bool SingleApplicationInstance::IsNumericString(const QString& strValue) const
{
    foreach (QChar character, strValue)
    {
        if (!character.isNumber())
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
/// Find a running process with a matching name excluding the a given process ID
/// (for example, excluding the current process ID).
/// On Linux,  the /proc folder contains numbered subfolders with information about
/// each process.  The numbered folder name is the process ID.  The ./cmdline file
/// within the numbered folder contains the command line, including the application
/// name.  This is parsed out and compared against the match string.
/// \param strMatchName The name of the process to search for.
/// \param excludedProcessID The processID to exclude from the search (-1 to ignore)
/// \return The processID of the matching process or -1 if none are found.
//-----------------------------------------------------------------------------
int SingleApplicationInstance::FindProcessId(const QString& strMatchName, int excludedProcessId) const
{
#ifdef Q_OS_WIN
    Q_UNUSED(strMatchName);
    Q_UNUSED(excludedProcessId);
    return -1;
#else

#ifdef _DEBUG_SINGLE_APP_INSTANCE_
    QString msg;
    QMessageBox(QMessageBox::Information, applicationName(), "SingleApplicationInstance::FindProcessId() - enter method.").exec();
#endif
    QDirIterator dirList("/proc", QDirIterator::NoIteratorFlags);
    while (dirList.hasNext())
    {
        dirList.next();
        if (IsNumericString(dirList.fileName()))
        {
            if (dirList.fileName().toInt() != excludedProcessId)
            {
                QString filePath;
                filePath = dirList.filePath() + QString("/cmdline");
                QFile commFile(filePath);
                if (commFile.open(QIODevice::ReadOnly))
                {
                    QTextStream cmdlineStream(&commFile);
                    QString     cmdline = cmdlineStream.readLine().toUtf8();

                    QFileInfo fileInfo(cmdline);
                    QString   processName = fileInfo.fileName().split(' ').first();
                    processName           = processName.trimmed();

                    if (processName == strMatchName)
                    {
#ifdef _DEBUG_SINGLE_APP_INSTANCE_
                        msg = "SingleApplicationInstance::FindProcessId() - found matching process '" + processName + "'" + "     '" + cmdline +
                              "' processId = " + dirList.fileName();
                        QMessageBox(QMessageBox::Information, applicationName(), msg).exec();
#endif
                        return dirList.fileName().toInt();
                    }
                }
            }
        }
    }
    return -1;
#endif
}
