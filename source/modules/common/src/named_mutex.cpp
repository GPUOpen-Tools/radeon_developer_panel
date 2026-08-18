// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of a named mutex class. The mutex is
///         global systemwide, so can be accessed across threads and processes
///         by its name. Each process will need its own copy of the mutex

#include "named_mutex.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>  // For O_* constants
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>  // For mode constants

#define PS_MAX_PATH (260)

#endif

#ifndef NM_UNUSED
#define NM_UNUSED(x) (static_cast<void>(x))
#endif

/// Base Implementation abstract data type
class NamedMutexImpl
{
public:
    /// Constructor
    NamedMutexImpl() {};

    /// Destructor
    virtual ~NamedMutexImpl() {};

    //-----------------------------------------------------------------------------
    /// Open an existing systemwide global mutex if it exists already, or create
    /// a new one if it doesn't exist
    /// \param mutexName the name this mutex will be known by
    /// \param initialOwner if true, the calling process will own this mutex, causing
    /// it to be started in the locked state
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    /// \return true if OpenOrCreate succeeded, false if error
    //-----------------------------------------------------------------------------
    virtual bool OpenOrCreate(const char* mutexName, bool initialOwner, bool global) = 0;

    //-----------------------------------------------------------------------------
    /// Open a previously created systemwide global mutex
    /// \param mutexName the name this mutex is be known by
    /// \param inherit if true, processes created by this process will inherit
    /// the mutex
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    /// \return true if Open succeeded, false if mutex doesn't exist
    //-----------------------------------------------------------------------------
    virtual bool Open(const char* mutexName, bool inherit, bool global) = 0;

    //-----------------------------------------------------------------------------
    /// Attempt to lock the mutex
    /// \return true if the Lock succeeded, false if not
    //-----------------------------------------------------------------------------
    virtual bool Lock() = 0;

    //-----------------------------------------------------------------------------
    /// Unlock a previously locked mutex
    //-----------------------------------------------------------------------------
    virtual void Unlock() = 0;

    //-----------------------------------------------------------------------------
    /// Close the mutex
    //-----------------------------------------------------------------------------
    virtual void Close() = 0;

private:
    //-----------------------------------------------------------------------------
    /// copy constructor made private; Cannot make copies of this object
    //-----------------------------------------------------------------------------
    NamedMutexImpl(const NamedMutexImpl& rhs) = delete;

    //-----------------------------------------------------------------------------
    /// assignment operator made private; Cannot make copies of this object
    //-----------------------------------------------------------------------------
    NamedMutexImpl& operator=(const NamedMutexImpl& rhs) = delete;
};

/// Windows-specific implementation
#if defined(_WIN32)
class NamedMutexWindows : public NamedMutexImpl
{
public:
    /// default constructor
    NamedMutexWindows()
        : m_hMutex(nullptr)
    {
    }

    /// destructor
    virtual ~NamedMutexWindows()
    {
        Close();
    }

    //--------------------------------------------------------------------------
    /// Open an existing systemwide global mutex if it exists already, or create
    /// a new one if it doesn't exist
    /// \param mutexName the name this mutex will be known by
    /// \param initialOwner if true, the calling process will own this mutex, causing
    /// it to be started in the locked state
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    ///
    /// \return true if OpenOrCreate succeeded, false if error
    //--------------------------------------------------------------------------
    virtual bool OpenOrCreate(const char* mutexName, bool initialOwner, bool global)
    {
        NM_UNUSED(global);

        if (m_hMutex == nullptr)
        {
            // CreateMutex will create the mutex if it can't be opened
            m_hMutex = CreateMutexA(nullptr,       // security attributes
                                    initialOwner,  // initial ownership?
                                    mutexName);    // name

            if (m_hMutex == nullptr)
            {
                return false;
            }
            else
            {
            }
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /// Open a previously created systemwide global mutex
    /// \param mutexName the name this mutex is be known by
    /// \param inherit if true, processes created by this process will inherit
    /// the mutex
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    ///
    /// \return true if Open succeeded, false if mutex doesn't exist
    //--------------------------------------------------------------------------
    virtual bool Open(const char* mutexName, bool inherit, bool global)
    {
        NM_UNUSED(global);

        m_hMutex = OpenMutexA(MUTEX_ALL_ACCESS,  // security attributes
                              inherit,           // inherit handle?
                              mutexName);        // name

        if (m_hMutex == nullptr)
        {
            return false;
        }
        else
        {
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /// Attempt to lock the mutex
    ///
    /// \return true if the Lock succeeded, false if not
    //--------------------------------------------------------------------------
    virtual bool Lock()
    {
        if (WAIT_OBJECT_0 != WaitForSingleObject(m_hMutex, INFINITE))
        {
            return false;
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /// Unlock a previously locked mutex
    //--------------------------------------------------------------------------
    virtual void Unlock()
    {
        ReleaseMutex(m_hMutex);
    }

    //--------------------------------------------------------------------------
    /// Close the mutex
    //--------------------------------------------------------------------------
    virtual void Close()
    {
        if (m_hMutex != nullptr)
        {
            CloseHandle(m_hMutex);
            m_hMutex = nullptr;
        }
    }

private:
    HANDLE m_hMutex;  ///< Windows handle to mutex
};

#endif

#if defined(__linux__) || defined(__APPLE__)
/// Linux (POSIX) implementation
/// Uses a POSIX semaphore to emulate a mutex, since semaphores are system-wide and
/// provide the naming required to identify individual mutexes

// Because of the way that Linux implements condition variables and mutexes
// (with shared memory), 32 and 64-bit versions need to be unique, since the
// shared memory consists of a data structure whose size is dependent on
// the bitsize.
#if AMDT_ADDRESS_SPACE_TYPE == AMDT_64_BIT_ADDRESS_SPACE
static const char* EXT = "_x64";
#else
static const char* EXT = "_x86";
#endif

static const int s_Mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

class NamedMutexPosix : public NamedMutexImpl
{
public:
    /// default constructor
    NamedMutexPosix()
        : m_mutex(nullptr)
        , m_threadID(0)
        , m_lockCount(0)
        , m_owner(false)
    {
    }

    /// destructor
    virtual ~NamedMutexPosix()
    {
        Close();
    }

    //--------------------------------------------------------------------------
    /// Open an existing systemwide global mutex if it exists already, or create
    /// a new one if it doesn't exist
    /// \param mutexName the name this mutex will be known by
    /// \param initialOwner if true, the calling process will own this mutex, causing
    /// it to be started in the locked state
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    ///
    /// \return true if OpenOrCreate succeeded, false if error
    //--------------------------------------------------------------------------
    virtual bool OpenOrCreate(const char* mutexName, bool initialOwner, bool global)
    {
        if (m_mutex == nullptr)
        {
            char name[PS_MAX_PATH];

            if (global)
            {
                snprintf(name, PS_MAX_PATH, "/%s", mutexName);
            }
            else
            {
                snprintf(name, PS_MAX_PATH, "/%s%s", mutexName, EXT);
            }

            // try to create the mutex first
            m_mutex = sem_open(name, O_CREAT | O_EXCL, s_Mode, 1);
            m_owner = true;

            if (m_mutex == SEM_FAILED)
            {
                // create failed. Mutex already exists, so just open it
                m_mutex = sem_open(name, 0);
                m_owner = false;

                if (m_mutex == SEM_FAILED)
                {
                    return false;
                }
            }

            strcpy(m_mutexName, name);
        }

        if (initialOwner)
        {
            return Lock();
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /// Open a previously created systemwide global mutex
    /// \param mutexName the name this mutex is be known by
    /// \param inherit if true, processes created by this process will inherit
    /// the mutex
    /// \param global if true, the mutex is shared between 32 and 64 bit versions. If
    /// false, the 32 and 64 bit versions will have independent mutexes
    ///
    /// \return true if Open succeeded, false if mutex doesn't exist
    //--------------------------------------------------------------------------
    virtual bool Open(const char* mutexName, bool inherit, bool global)
    {
        NM_UNUSED(inherit);

        if (m_mutex == nullptr)
        {
            char name[PS_MAX_PATH];

            if (global)
            {
                snprintf(name, PS_MAX_PATH, "/%s", mutexName);
            }
            else
            {
                snprintf(name, PS_MAX_PATH, "/%s%s", mutexName, EXT);
            }

            // open the mutex
            m_mutex = sem_open(name, 0);

            if (m_mutex != SEM_FAILED)
            {
                strcpy(m_mutexName, name);
                return true;
            }

            return false;
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /// Attempt to lock the mutex
    ///
    /// \return true if the Lock succeeded, false if not
    //--------------------------------------------------------------------------
    virtual bool Lock()
    {
        // if this thread currently has the lock, increment the lock count
        // lock count is initialized in constructor and when unlocking
        pthread_t threadID = osGetCurrentThreadId();

        if (threadID == m_threadID)
        {
            m_lockCount++;
            return true;
        }

        if (sem_wait(m_mutex) == 0)  // lock the mutex and check for error
        {
#ifndef __APPLE__  // sem_getvalue() not implemented on Mac
            int value;
            sem_getvalue(m_mutex, &value);
#endif
            m_threadID = threadID;
            return true;
        }

        return false;
    }

    //--------------------------------------------------------------------------
    /// Unlock a previously locked mutex
    //--------------------------------------------------------------------------
    virtual void Unlock()
    {
        pthread_t threadID = osGetCurrentThreadId();

        if (threadID == m_threadID)
        {
            m_lockCount--;
        }

        if (m_lockCount < 0)
        {
            m_lockCount = 0;
            m_threadID  = 0;
            sem_post(m_mutex);  // unlock the mutex
        }
    }

    //--------------------------------------------------------------------------
    /// Close the mutex
    //--------------------------------------------------------------------------
    virtual void Close()
    {
        if (m_mutex != nullptr)
        {
            sem_close(m_mutex);
            m_mutex     = nullptr;
            m_lockCount = 0;
            m_threadID  = 0;

            if (m_owner)
            {
                sem_unlink(m_mutexName);
                m_owner = false;
            }
        }
    }

private:
    //--------------------------------------------------------------------------
    /// Get the current thread ID
    /// \return the current thread ID
    //--------------------------------------------------------------------------
    pthread_t osGetCurrentThreadId()
    {
        pthread_t retVal = 0;

        // Get the current pthread id:
        retVal = ::pthread_self();

        // For MAC build: Get the current Mach kernel thread id:
        // retVal = mach_thread_self();

        return retVal;
    }

    sem_t*    m_mutex;                   ///< pointer to mutex object
    pthread_t m_threadID;                ///< ID of the thread which currently owns the lock
    long      m_lockCount;               ///< number of times this thread has called lock (non-recursive mutex)
    char      m_mutexName[PS_MAX_PATH];  ///< name of mutex
    bool      m_owner;                   ///< did this object create the mutex
};

#endif

//--------------------------------------------------------------------------
/// Main Implementation methods.
/// default constructor
/// Pick an implementation based on platform
//--------------------------------------------------------------------------
NamedMutex::NamedMutex()
{
#if defined(_WIN32)
    m_pImpl = new NamedMutexWindows();
#else
    m_pImpl = new NamedMutexPosix();
#endif
}

/// destructor
NamedMutex::~NamedMutex()
{
    delete m_pImpl;
}

//--------------------------------------------------------------------------
/// Open an existing systemwide global mutex if it exists already, or create
/// a new one if it doesn't exist
/// \param mutexName the name this mutex will be known by
/// \param initialOwner if true, the calling process will own this mutex, causing
/// it to be started in the locked state
/// \param global if true, the mutex is shared between 32 and 64 bit versions. If
/// false, the 32 and 64 bit versions will have independent mutexes
///
/// \return true if OpenOrCreate succeeded, false if error
//--------------------------------------------------------------------------
bool NamedMutex::OpenOrCreate(const char* mutexName, bool initialOwner, bool global)
{
    return m_pImpl->OpenOrCreate(mutexName, initialOwner, global);
}

//--------------------------------------------------------------------------
/// Open a previously created systemwide global mutex
/// \param mutexName the name this mutex is be known by
/// \param inherit if true, processes created by this process will inherit
/// the mutex
/// \param global if true, the mutex is shared between 32 and 64 bit versions. If
/// false, the 32 and 64 bit versions will have independent mutexes
///
/// \return true if Open succeeded, false if mutex doesn't exist
//--------------------------------------------------------------------------
bool NamedMutex::Open(const char* mutexName, bool inherit, bool global)
{
    return m_pImpl->Open(mutexName, inherit, global);
}

//--------------------------------------------------------------------------
/// Attempt to lock the mutex
///
/// \return true if the Lock succeeded, false if not
//--------------------------------------------------------------------------
bool NamedMutex::Lock()
{
    return m_pImpl->Lock();
}

//--------------------------------------------------------------------------
/// Unlock a previously locked mutex
//--------------------------------------------------------------------------
void NamedMutex::Unlock()
{
    m_pImpl->Unlock();
}

//--------------------------------------------------------------------------
/// Close the mutex
//--------------------------------------------------------------------------
void NamedMutex::Close()
{
    m_pImpl->Close();
}
