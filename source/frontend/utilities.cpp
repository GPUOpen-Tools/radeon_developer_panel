// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility function implementations for RDP

#include "utilities.h"

#include <array>
#include <cstring>

#ifdef _WIN32
#define RDP_WIN32_THREAD_NAME_MAX 256
#include <Windows.h>

#include <cstdlib>
#elif defined(__APPLE__)
#include <pthread.h>
#elif defined(__gnu_linux__)
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QString>

namespace rdp
{
    namespace util
    {
        static constexpr size_t kRegistryValueNumBytes     = 128;    ///< The number of bytes to allocate for the buffer to put registry values into.
        static constexpr int    kWindows11BuildNumberStart = 22000;  ///< Any windows build greater than this is at least Windows 11.

        uint64_t GetCurrentThreadIdentifier()
        {
            uint64_t tid;
#ifdef _WIN32
            tid = static_cast<uint64_t>(GetCurrentThreadId());
#elif defined(__APPLE__)
            pthread_threadid_np(nullptr, &tid);
#elif defined(__gnu_linux__)
            static_assert(sizeof(pid_t) <= sizeof(uint64_t), "Thread ID cannot be represented by uint64_t on this platform!");
            tid = syscall(SYS_gettid);
#endif
            return tid;
        }

        void SetCurrentThreadName(const char* name)
        {
#ifdef _WIN32
            size_t                                         converted = 0;
            std::array<wchar_t, RDP_WIN32_THREAD_NAME_MAX> wide_name{};
            mbstowcs_s(&converted, wide_name.data(), wide_name.size(), name, wide_name.size() - 1);
            SetThreadDescription(GetCurrentThread(), wide_name.data());
#elif defined(__APPLE__)
            pthread_setname_np(name);
#elif defined(__gnu_linux__)
            pthread_t self = pthread_self();
            pthread_setname_np(self, name);
#else
            Q_UNUSED(name)
#endif
        }

        void GetCurrentThreadName(char* buffer, size_t size)
        {
            memset(buffer, 0, size);
#ifdef _WIN32
            HANDLE  thread_handle = GetCurrentThread();
            PWSTR   thread_desc;
            HRESULT result = GetThreadDescription(thread_handle, &thread_desc);
            if (SUCCEEDED(result))
            {
                std::wcstombs(buffer, thread_desc, size);
            }
#elif defined(__APPLE__) || defined(__gnu_linux__)
            pthread_t self = pthread_self();
            pthread_getname_np(self, buffer, size);
#endif
        }

        void DebugPrintLine(const char* msg)
        {
#ifdef _WIN32
            OutputDebugStringA(msg);
            OutputDebugStringA("\n");
#else
            (void)msg;
#endif
        }

        QDir GetApplicationDataPath()
        {
            // Check if app data directory doesn't currently exist
            QString appdata_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir    appdata_dir(appdata_path);
            if (!appdata_dir.exists())
            {
                appdata_dir.mkpath(".");
            }

            return appdata_dir;
        }

        QString PrettyOsName()
        {
#ifdef Q_OS_WINDOWS
            HKEY h_key = nullptr;
            LONG res   = RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &h_key);
            if (res == ERROR_SUCCESS)
            {
                DWORD                                    key_type   = 0;
                DWORD                                    value_size = kRegistryValueNumBytes;
                std::array<char, kRegistryValueNumBytes> text_buffer{};
                res = RegQueryValueExA(h_key, "CurrentBuildNumber", nullptr, &key_type, reinterpret_cast<LPBYTE>(text_buffer.data()), &value_size);
                if (res == ERROR_SUCCESS)
                {
                    const int radix        = 10;
                    ULONG     build_number = strtoul(text_buffer.data(), nullptr, radix);
                    if (build_number >= kWindows11BuildNumberStart)
                    {
                        res = RegQueryValueExA(h_key, "DisplayVersion", nullptr, &key_type, reinterpret_cast<LPBYTE>(text_buffer.data()), &value_size);
                        if (res == ERROR_SUCCESS)
                        {
                            return QString("Windows 11 Version %1").arg(text_buffer.data());
                        }
                    }
                }
            }
#endif

            return QSysInfo::prettyProductName();
        }
    }  // namespace util

}  // namespace rdp
