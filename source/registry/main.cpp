// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Main entry point for Enable Sync Primitives utility

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <ddAmdGpuInfo.h>

#include <format>
#include <iostream>

constexpr const wchar_t* kRegKeyPrefix  = L"SYSTEM\\CurrentControlSet\\Services";
constexpr const wchar_t* kRegKeySuffix  = L"Parameters";
constexpr const wchar_t* kRegistryValue = L"RgpEnableEtw";

namespace
{
    bool HasRegKeyAndValue(HKEY parent, LPCWSTR key, LPCWSTR value)
    {
        HKEY    out;
        LSTATUS status = RegOpenKeyExW(parent, key, 0, KEY_READ, &out);
        if (status != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD data_type;
        DWORD data      = 0;
        DWORD data_size = sizeof(data);
        status          = RegQueryValueExW(out, value, NULL, &data_type, reinterpret_cast<BYTE*>(&data), &data_size);
        RegCloseKey(out);

        if (status != ERROR_SUCCESS || data_type != REG_DWORD || data != 1)
        {
            return false;
        }

        return true;
    }

}  // namespace

int main(int argc, char* argv[])
{
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    bool cleanup = false;
    if (argc >= 2)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (!strcmp(argv[i], "--cleanup"))
            {
                cleanup = true;
            }
        }
    }

    const auto key               = DevDriver::QueryServiceString();
    const auto full_reg_key_path = std::format(L"{}\\{}\\{}", kRegKeyPrefix, key, kRegKeySuffix);
    const bool has_reg_key_value = HasRegKeyAndValue(HKEY_LOCAL_MACHINE, full_reg_key_path.c_str(), kRegistryValue);
    if (cleanup)
    {
        if (has_reg_key_value)
        {
            LSTATUS error = RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, full_reg_key_path.c_str(), kRegistryValue);
            if (error != ERROR_SUCCESS)
            {
                LPSTR  message = nullptr;
                size_t message_length =
                    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, (LPSTR)&message, 0, nullptr);
                const auto exception = std::format("Failed to delete registry key: {}", message);
                LocalFree(message);

                std::cerr << exception << std::endl;
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr << "Registry key not found" << std::endl;
        }
    }
    else if (!has_reg_key_value)
    {
        const auto full_key_value_path = std::format(L"{}\\{}", full_reg_key_path, kRegistryValue);
        HKEY       key;
        LSTATUS    error = RegCreateKeyW(HKEY_LOCAL_MACHINE, full_reg_key_path.c_str(), &key);
        if (error != ERROR_SUCCESS)
        {
            LPSTR  message        = nullptr;
            size_t message_length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, (LPSTR)&message, 0, nullptr);
            const auto exception  = std::format("Failed to create registry key: {}", message);
            LocalFree(message);

            std::cerr << exception << std::endl;
            return EXIT_FAILURE;
        }

        DWORD value = 1;
        error       = RegSetValueExW(key, kRegistryValue, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
        if (error != ERROR_SUCCESS)
        {
            LPSTR  message        = nullptr;
            size_t message_length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, (LPSTR)&message, 0, nullptr);
            const auto exception  = std::format("Failed to set registry key value: {}", message);
            LocalFree(message);

            std::cerr << exception << std::endl;
            return EXIT_FAILURE;
        }
    }
    else
    {
        return 2;
    }

    return 0;
}
