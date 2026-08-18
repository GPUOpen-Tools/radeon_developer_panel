// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for API blocklist.

#ifndef RDP_SOURCE_API_CAPTURE_API_BLOCKLIST_H_
#define RDP_SOURCE_API_CAPTURE_API_BLOCKLIST_H_

#include <mutex>
#include <string>
#include <vector>

#include "RdpCaptureApi.h"

/// @brief Manages a list of process name patterns that are blocked from connecting.
///
/// Patterns support the following wildcard syntax:
///   - '*'     matches zero or more of any character
///   - '?'     matches exactly one character
///   - '[abc]' matches one character in the set; '[!abc]' matches one character NOT in the set
///   - '[a-z]' matches one character in the range a–z
///   - '\x'    treats the next character x as a literal (backslash escaping)
///
/// Matching is performed against the process filename (basename), not the full path.
/// Matching is case-insensitive on Windows and case-sensitive on Linux.
class ApiBlocklist
{
public:
    /// @brief Constructor. Loads platform-specific default entries.
    ApiBlocklist();

    /// @brief Adds a process name pattern to the blocklist.
    ///
    /// Has no effect if the pattern is already present.
    /// @param [in] pattern The wildcard pattern to add (e.g. "svchost.exe", "MyApp*").
    /// @return kRdpCaptureResultSuccess, or kRdpCaptureResultInvalidParams if pattern is null/empty.
    RdpCaptureResult AddEntry(const char* pattern);

    /// @brief Removes a process name pattern from the blocklist.
    /// @param [in] pattern The pattern to remove.
    /// @return kRdpCaptureResultSuccess if removed, kRdpCaptureResultNotFound if not present,
    ///         or kRdpCaptureResultInvalidParams if pattern is null.
    RdpCaptureResult RemoveEntry(const char* pattern);

    /// @brief Clears all blocklist entries, including built-in defaults.
    void Clear();

    /// @brief Loads additional blocklist entries from a text file.
    ///
    /// File format: one pattern per line; lines starting with '#' are comments.
    /// Duplicate entries are silently ignored.
    /// @param [in] file_path Path to the blocklist file.
    /// @return kRdpCaptureResultSuccess, kRdpCaptureResultNotFound if the file cannot be opened,
    ///         or kRdpCaptureResultInvalidParams if file_path is null/empty.
    RdpCaptureResult LoadFile(const char* file_path);

    /// @brief Returns all current blocklist patterns.
    ///
    /// The returned array and each string must be freed with FreeEntries().
    /// @param [out] entries_out  Array of null-terminated pattern strings.
    /// @param [out] num_entries  Number of entries written to entries_out.
    void GetEntries(char*** entries_out, uint64_t* num_entries) const;

    /// @brief Frees memory returned by GetEntries().
    /// @param [in] entries     The array returned by GetEntries().
    /// @param [in] num_entries The number of entries in the array.
    static void FreeEntries(char** entries, uint64_t num_entries);

    /// @brief Returns true if the given process should be blocked.
    ///
    /// Extracts the basename of process_path and checks it against all stored patterns.
    /// @param [in] process_path The full path (or name) of the connecting process.
    /// @return true if a blocklist pattern matches the process filename.
    bool Contains(const std::string& process_path) const;

    /// @brief Registers a callback invoked whenever a process is blocked.
    ///
    /// Pass nullptr to unregister the callback.
    /// @param [in] callback   Function called with (user_data, full_process_path, process_id).
    /// @param [in] user_data  Opaque pointer forwarded to the callback.
    void SetBlockedCallback(void (*callback)(void* user_data, const char* process_path, uint32_t process_id), void* user_data);

    /// @brief Invokes the blocked callback if one is registered.
    /// @param [in] process_path Full path of the blocked process.
    /// @param [in] process_id   PID of the blocked process.
    void NotifyBlocked(const char* process_path, uint32_t process_id) const;

private:
    /// @brief Extracts the filename component from a full path.
    static std::string GetBasename(const std::string& path);

    /// @brief Returns true if name matches pattern using '*' / '?' wildcards.
    static bool MatchesWildcard(const std::string& name, const std::string& pattern);

    mutable std::mutex       mutex_;
    std::vector<std::string> patterns_;

    void (*blocked_callback_)(void*, const char*, uint32_t) = nullptr;
    void* blocked_user_data_                                = nullptr;
};

#endif
