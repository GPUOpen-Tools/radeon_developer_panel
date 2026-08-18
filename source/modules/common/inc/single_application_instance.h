// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  QApplication subclass with support for duplicate process checking

#ifndef RDP_MODULES_COMMON_SINGLE_APPLICATION_INSTANCE_H_
#define RDP_MODULES_COMMON_SINGLE_APPLICATION_INSTANCE_H_

#include <QApplication>
#include <QSharedMemory>
#include <QStringList>

class SingleInstance;

class SingleApplicationInstance : public QApplication
{
    Q_OBJECT
public:
    SingleApplicationInstance(int& argc, char* argv[], const QString uniqueId, bool checkHeadlessInstances = false);
    virtual ~SingleApplicationInstance();
    bool IsAnotherInstanceRunning();
    bool IsInstanceRunning(const QString& uniqueKey) const;
    bool IsPrimaryInstance();
    bool NotifyAppInstanceStarted();

public slots:
    void OnCheckForNewInstance();

    // Reimplemented from QApplication so we can throw exceptions in slots
    virtual bool notify(QObject* receiver, QEvent* event);

signals:
    void AppInstanceStarted();

private:
    bool CreateSharedMemory();
    bool IsNumericString(const QString& strValue) const;
    int  FindProcessId(const QString& strMatchName, int excludedProcessId) const;
    void DisplayCharacters(const QString& strValue) const;

private:
    QString         m_uniqueKey;               ///< GUID Key for this application
    bool            m_anotherInstanceRunning;  ///< A bool that tracks if another instance is already running.
    QSharedMemory   m_sharedMem;               ///< Shared memory that secondary instance uses to notify primary instance.
    SingleInstance* m_pSingleInstance;         ///< Instance detection mutex for non-gui applications
};

#endif
