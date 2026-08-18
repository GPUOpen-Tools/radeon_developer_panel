// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Global shortcut structure definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_H_
#define RDP_SOURCE_MODULES_COMMON_INC_GLOBAL_SHORTCUT_H_

/// @brief Defines a global shortcut
struct GlobalShortcut
{
    int id;          ///< The shortcut identifier
    int sequence;    ///< The shortcut key sequence
    int native_key;  ///< The native shortcut key (without modifiers)

    /// @brief Default constructor
    GlobalShortcut()
    {
        id         = 0;
        sequence   = 0;
        native_key = 0;
    }

    /// @brief Constructs shortcut
    /// @param [in] id The shortcut id
    /// @param [in] sequence The shortcut key sequence
    /// @param [in] native_key The native shortcut key
    GlobalShortcut(int id, int sequence, int native_key)
        : id(id)
        , sequence(sequence)
        , native_key(native_key)
    {
    }

    /// @brief Copy constructor
    /// @param [in] other The shortcut to copy from
    GlobalShortcut(const GlobalShortcut& other)
    {
        this->id         = other.id;
        this->sequence   = other.sequence;
        this->native_key = other.native_key;
    }

    /// @brief Copy assignment operator
    /// @param [in] other The shortcut to copy
    /// @return copied shortcut
    GlobalShortcut& operator=(GlobalShortcut other)
    {
        std::swap(other.id, id);
        std::swap(other.sequence, sequence);
        std::swap(other.native_key, native_key);

        return *this;
    }

    /// @brief Compares shortcuts
    /// @param [in] other The shortcut to compare against
    /// @return true if shortcut ids match
    bool operator==(const GlobalShortcut& other) const
    {
        return id == other.id;
    }

    /// @brief Compares id values
    /// @param [in] other_id The id to compare
    /// @return true if id values match
    bool operator==(int other_id) const
    {
        return this->id == other_id;
    }
};

#endif
