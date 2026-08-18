// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for API blocklist.

#include "api_blocklist.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>

#ifdef WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include "api_allocator.h"

namespace
{
#ifdef WIN32
    constexpr bool        kCaseSensitive    = false;
    constexpr const char* kBlocklistSection = "Win32";
#elif defined(__linux__)
    constexpr bool        kCaseSensitive    = true;
    constexpr const char* kBlocklistSection = "Linux";
#else
    constexpr bool        kCaseSensitive    = true;
    constexpr const char* kBlocklistSection = "";
#endif

    // Fallback defaults used when blocklist.ini cannot be found at runtime.
    // Content mirrors source/frontend/blocklist.ini — update that file first,
    // then sync here if the file-based path is unavailable.
    constexpr const char* kFallbackEntriesWin32[] = {
        "svchost.exe",
        "RadeonSettings.exe",
        "RadeonSoftware.exe",
        "taskhost.exe",
        "taskhostw.exe",
        "Taskmgr.exe",
        "amddvr.exe",
        "dwm.exe",
        "dxgiadaptercache.exe",
        "steamwebhelper.exe",
        "AMDRSServ.exe",
        "DeviceCensus.exe",
        "Code.exe",
        "msedge.exe",
        "chrome.exe",
        "Skype.exe",
        "RadeonInstaller.exe",
        "Teams.exe",
        "RadeonGPUAnalyzer.exe",
        "amdspv.exe",
        "rga.exe",
        "dx12_backend.exe",
        "vulkan_backend.exe",
        "vulkandriverquery.exe",
        "vulkandriverquery64.exe",
        "RadeonRaytracingAnalyzer*",
        "msedgewebview2.exe",
        "dxdiag.exe",
        "clinfo.exe",
        "EpicWebHelper.exe",
        "EpicGamesLauncher.exe",
        "AMDRSSrcExt.exe",
        "GPUQuery_External*",
        "cncmd.exe",
    };

    constexpr const char* kFallbackEntriesLinux[] = {
        "RadeonGPUAnalyzer",
        "amdspv",
        "vulkan_backend",
        "rga",
        "vulkandriverquery",
        "fossilize_replay",
        "RadeonRaytracingAnalyzer*",
    };

    char ToLowerChar(const char c)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    /// @brief Returns the directory containing this DLL or executable.
    std::string GetModuleDirectory()
    {
#ifdef WIN32
        char    path[MAX_PATH] = {};
        HMODULE module         = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&GetModuleDirectory),
                               &module) &&
            module != nullptr)
        {
            GetModuleFileNameA(module, path, MAX_PATH);
            const std::string full_path(path);
            const size_t      sep = full_path.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                return full_path.substr(0, sep + 1);
            }
        }
        return {};
#elif defined(__linux__)
        Dl_info info{};
        if (dladdr(reinterpret_cast<void*>(&GetModuleDirectory), &info) != 0 && info.dli_fname != nullptr)
        {
            const std::string full_path(info.dli_fname);
            const size_t      sep = full_path.rfind('/');
            if (sep != std::string::npos)
            {
                return full_path.substr(0, sep + 1);
            }
        }
        return {};
#else
        return {};
#endif
    }

    /// @brief Parses entries from a blocklist.ini (QSettings-style INI) into patterns.
    ///
    /// Reads entries from the platform section ([Win32] or [Linux]).
    /// Lines of the form "applications\N\name = pattern" are added to patterns.
    /// @return true if the file was opened and at least one entry was read.
    bool LoadFromIniFile(const std::string& file_path, std::vector<std::string>& patterns)
    {
        std::ifstream file(file_path);
        if (!file.is_open())
        {
            return false;
        }

        bool        in_section = false;
        bool        any_read   = false;
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty())
            {
                continue;
            }

            if (line[0] == '[')
            {
                const size_t end = line.find(']');
                if (end != std::string::npos)
                {
                    in_section = (line.substr(1, end - 1) == kBlocklistSection);
                }
                continue;
            }

            if (!in_section)
            {
                continue;
            }

            // Expect lines like: applications\N\name = pattern
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            std::string key = line.substr(0, eq);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            {
                key.pop_back();
            }
            constexpr std::string_view kNameSuffix = "\\name";
            if (key.size() < kNameSuffix.size() || key.substr(key.size() - kNameSuffix.size()) != kNameSuffix)
            {
                continue;
            }

            std::string  value = line.substr(eq + 1);
            const size_t first = value.find_first_not_of(" \t");
            const size_t last  = value.find_last_not_of(" \t");
            if (first == std::string::npos)
            {
                continue;
            }
            value = value.substr(first, last - first + 1);

            if (!value.empty() && std::ranges::find(patterns, value) == patterns.end())
            {
                patterns.emplace_back(value);
                any_read = true;
            }
        }
        return any_read;
    }

    void LoadFallbackDefaults(std::vector<std::string>& patterns)
    {
#ifdef WIN32
        for (const char* entry : kFallbackEntriesWin32)
        {
            patterns.emplace_back(entry);
        }
#elif defined(__linux__)
        for (const char* entry : kFallbackEntriesLinux)
        {
            patterns.emplace_back(entry);
        }
#endif
    }
}  // namespace

ApiBlocklist::ApiBlocklist()
{
    const std::string dir      = GetModuleDirectory();
    const std::string ini_path = dir + "blocklist.ini";
    if (!LoadFromIniFile(ini_path, patterns_))
    {
        LoadFallbackDefaults(patterns_);
    }
}

RdpCaptureResult ApiBlocklist::AddEntry(const char* pattern)
{
    if (pattern == nullptr || pattern[0] == '\0')
    {
        return kRdpCaptureResultInvalidParams;
    }

    const std::lock_guard lock(mutex_);
    const auto            matches = [&](const std::string& existing) {
        if constexpr (kCaseSensitive)
        {
            return existing == pattern;
        }
        else
        {
            if (existing.size() != std::strlen(pattern))
            {
                return false;
            }
            return std::equal(existing.begin(), existing.end(), pattern, [](const char a, const char b) { return ToLowerChar(a) == ToLowerChar(b); });
        }
    };
    if (std::ranges::find_if(patterns_, matches) == patterns_.end())
    {
        patterns_.emplace_back(pattern);
    }
    return kRdpCaptureResultSuccess;
}

RdpCaptureResult ApiBlocklist::RemoveEntry(const char* pattern)
{
    if (pattern == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    const std::lock_guard lock(mutex_);
    const auto            matches = [&](const std::string& existing) {
        if constexpr (kCaseSensitive)
        {
            return existing == pattern;
        }
        else
        {
            if (existing.size() != std::strlen(pattern))
            {
                return false;
            }
            return std::equal(existing.begin(), existing.end(), pattern, [](const char a, const char b) { return ToLowerChar(a) == ToLowerChar(b); });
        }
    };
    const auto it = std::ranges::find_if(patterns_, matches);
    if (it == patterns_.end())
    {
        return kRdpCaptureResultNotFound;
    }
    patterns_.erase(it);
    return kRdpCaptureResultSuccess;
}

void ApiBlocklist::Clear()
{
    const std::lock_guard lock(mutex_);
    patterns_.clear();
}

RdpCaptureResult ApiBlocklist::LoadFile(const char* file_path)
{
    if (file_path == nullptr || file_path[0] == '\0')
    {
        return kRdpCaptureResultInvalidParams;
    }

    std::ifstream file(file_path);
    if (!file.is_open())
    {
        return kRdpCaptureResultNotFound;
    }

    std::vector<std::string> new_patterns;
    std::string              line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            continue;
        }
        const size_t last = line.find_last_not_of(" \t");
        line              = line.substr(first, last - first + 1);

        if (line[0] == '#')
        {
            continue;
        }

        if (!line.empty())
        {
            new_patterns.emplace_back(line);
        }
    }

    const std::lock_guard lock(mutex_);
    for (auto& pattern : new_patterns)
    {
        if (std::ranges::find(patterns_, pattern) == patterns_.end())
        {
            patterns_.emplace_back(std::move(pattern));
        }
    }

    return kRdpCaptureResultSuccess;
}

void ApiBlocklist::GetEntries(char*** entries_out, uint64_t* num_entries) const
{
    if (entries_out == nullptr || num_entries == nullptr)
    {
        return;
    }

    const std::lock_guard lock(mutex_);
    *num_entries = static_cast<uint64_t>(patterns_.size());
    if (patterns_.empty())
    {
        *entries_out = nullptr;
        return;
    }

    char** entries = static_cast<char**>(ApiAlloc(patterns_.size() * sizeof(char*)));
    for (size_t i = 0; i < patterns_.size(); ++i)
    {
        const size_t len = patterns_[i].size() + 1;
        entries[i]       = static_cast<char*>(ApiAlloc(len));
        std::memcpy(entries[i], patterns_[i].c_str(), len);
    }
    *entries_out = entries;
}

void ApiBlocklist::FreeEntries(char** entries, uint64_t num_entries)
{
    if (entries == nullptr)
    {
        return;
    }
    for (uint64_t i = 0; i < num_entries; ++i)
    {
        ApiFree(entries[i]);
    }
    ApiFree(entries);
}

bool ApiBlocklist::Contains(const std::string& process_path) const
{
    const std::string basename = GetBasename(process_path);
    if (basename.empty())
    {
        return false;
    }

    std::string name_to_match = basename;
    if constexpr (!kCaseSensitive)
    {
        std::ranges::transform(name_to_match, name_to_match.begin(), ToLowerChar);
    }

    const std::lock_guard lock(mutex_);
    for (const auto& pattern : patterns_)
    {
        std::string pat = pattern;
        if constexpr (!kCaseSensitive)
        {
            std::ranges::transform(pat, pat.begin(), ToLowerChar);
        }
        if (MatchesWildcard(name_to_match, pat))
        {
            return true;
        }
    }
    return false;
}

std::string ApiBlocklist::GetBasename(const std::string& path)
{
    if (path.empty())
    {
        return {};
    }
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        return path;
    }
    return path.substr(pos + 1);
}

bool ApiBlocklist::MatchesWildcard(const std::string& name, const std::string& pattern)
{
    const char* n      = name.c_str();
    const char* p      = pattern.c_str();
    const char* star_p = nullptr;
    const char* star_n = nullptr;

    while (*n != '\0')
    {
        if (*p == '\\' && *(p + 1) != '\0')
        {
            if (*n != *(p + 1))
            {
                if (star_p == nullptr)
                    return false;
                p = star_p;
                n = ++star_n;
                continue;
            }
            p += 2;
            ++n;
        }
        else if (*p == '[')
        {
            const char* class_p = p + 1;
            const bool  negate  = (*class_p == '!');
            if (negate)
                ++class_p;

            const char* class_end = class_p;
            if (*class_end == ']')
                ++class_end;
            while (*class_end != '\0' && *class_end != ']')
                ++class_end;

            if (*class_end != ']')
            {
                if (*n != '[')
                {
                    if (star_p == nullptr)
                        return false;
                    p = star_p;
                    n = ++star_n;
                    continue;
                }
                ++p;
                ++n;
            }
            else
            {
                bool matched = false;
                for (const char* q = class_p; q < class_end;)
                {
                    if (*(q + 1) == '-' && (q + 2) < class_end)
                    {
                        if (static_cast<unsigned char>(*n) >= static_cast<unsigned char>(*q) &&
                            static_cast<unsigned char>(*n) <= static_cast<unsigned char>(*(q + 2)))
                        {
                            matched = true;
                        }
                        q += 3;
                    }
                    else
                    {
                        if (*n == *q)
                            matched = true;
                        ++q;
                    }
                }
                if (negate)
                    matched = !matched;
                if (!matched)
                {
                    if (star_p == nullptr)
                        return false;
                    p = star_p;
                    n = ++star_n;
                    continue;
                }
                p = class_end + 1;
                ++n;
            }
        }
        else if (*p == '*')
        {
            star_p = ++p;
            star_n = n;
        }
        else if (*p == '?' || *p == *n)
        {
            ++p;
            ++n;
        }
        else
        {
            if (star_p == nullptr)
                return false;
            p = star_p;
            n = ++star_n;
        }
    }

    while (*p == '*')
        ++p;
    return *p == '\0';
}

void ApiBlocklist::SetBlockedCallback(void (*callback)(void* user_data, const char* process_path, uint32_t process_id), void* user_data)
{
    const std::lock_guard lock(mutex_);
    blocked_callback_  = callback;
    blocked_user_data_ = user_data;
}

void ApiBlocklist::NotifyBlocked(const char* process_path, const uint32_t process_id) const
{
    void (*cb)(void*, const char*, uint32_t) = nullptr;
    void* ud                                 = nullptr;
    {
        const std::lock_guard lock(mutex_);
        cb = blocked_callback_;
        ud = blocked_user_data_;
    }
    if (cb != nullptr)
    {
        cb(ud, process_path, process_id);
    }
}
