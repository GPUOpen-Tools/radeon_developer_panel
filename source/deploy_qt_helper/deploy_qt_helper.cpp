// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Helper for qt deployment that locks a file

#include <chrono>
#include <exception>
#include <iostream>
#include <sstream>

static constexpr const char* kLockfileName  = "deploy_qt.lock";
static constexpr uint32_t    kLockTimeoutMs = 20000;

#ifdef WIN32
#include "Windows.h"
#else
#include <sys/file.h>
#include <unistd.h>
#endif

int Execute(const std::string& command)
{
#ifdef WIN32
    HANDLE     handle      = ::CreateFileA(kLockfileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    const bool opened_file = handle != INVALID_HANDLE_VALUE;

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
#else
    int        file_descriptor = open(kLockfileName, O_RDWR, 0666);
    const bool opened_file     = file_descriptor != 0;
#endif

    if (!opened_file)
    {
        std::cerr << "Failed to open file" << std::endl;
        return -1;
    }

    bool acquired_lock = false;
    auto end           = std::chrono::system_clock::now() + std::chrono::milliseconds(kLockTimeoutMs);
    while (std::chrono::system_clock::now() < end && !acquired_lock)
    {
#ifdef WIN32
        acquired_lock |= LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlapped) != 0;
#else
        acquired_lock |= flock(file_descriptor, LOCK_EX | LOCK_NB) == 0;
#endif
    }

    if (!acquired_lock)
    {
        std::cerr << "Failed to acquire lock within timeout." << std::endl;
        return -2;
    }

    const int result = system(command.c_str());

#ifdef WIN32
    UnlockFileEx(handle, 0, 1, 0, &overlapped);
    CloseHandle(handle);
#else
    flock(file_descriptor, LOCK_UN);
    close(file_descriptor);
#endif

    return result;
}

int main(int argc, const char** argv)
{
    try
    {
        // Need at least one argument
        if (argc == 1)
        {
            return -1;
        }

        std::ostringstream command_stream;
        for (int i = 1; i < argc; i++)
        {
            command_stream << argv[i];
            if (i != 0 && i != argc - 1)
            {
                command_stream << " ";
            }
        }

        return Execute(command_stream.str());
    }
    catch (const std::exception&)
    {
        std::cerr << "Unexpected exception" << std::endl;
        return -1;
    }
}
