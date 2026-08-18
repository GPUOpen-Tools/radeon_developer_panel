// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for JSON mapper.

#ifndef RDP_SOURCE_TRACE_INC_JSON_JSON_MAPPER
#define RDP_SOURCE_TRACE_INC_JSON_JSON_MAPPER

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

// Ensure RAPIDJSON_ASSERT uses standard assert to be compatible with all expression types.
// The external ddJsonWriter.h redefines this to DD_ASSERT which has type restrictions.
#ifdef RAPIDJSON_ASSERT
#undef RAPIDJSON_ASSERT
#endif
#include <cassert>
#define RAPIDJSON_ASSERT(x) assert(x)
#include <rapidjson/document.h>

#include "dev_trace_common.h"

namespace devtrace
{
    namespace detail
    {
        // Type traits for detecting STL containers used with the scalar Map template.
        template <typename T>
        struct is_std_vector : std::false_type
        {
        };
        template <typename T, typename A>
        struct is_std_vector<std::vector<T, A>> : std::true_type
        {
        };
        template <typename T>
        inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

        template <typename T>
        struct is_std_unordered_set : std::false_type
        {
        };
        template <typename T, typename H, typename E, typename A>
        struct is_std_unordered_set<std::unordered_set<T, H, E, A>> : std::true_type
        {
        };
        template <typename T>
        inline constexpr bool is_std_unordered_set_v = is_std_unordered_set<T>::value;

        template <typename T>
        inline constexpr bool is_container_v = is_std_vector_v<T> || is_std_unordered_set_v<T>;

        /// @brief Reads a scalar value from a RapidJSON Value into the output variable.
        /// @tparam T The C++ type to read into.
        /// @param [in] json The RapidJSON value to read from.
        /// @param [out] out The variable to write the result to.
        /// @return true if the value was successfully read, false on type mismatch.
        template <typename T>
        bool GetValue(const rapidjson::Value& json, T& out)
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                if (!json.IsBool())
                    return false;
                out = json.GetBool();
                return true;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                if (!json.IsString())
                    return false;
                out.assign(json.GetString(), json.GetStringLength());
                return true;
            }
            else if constexpr (std::is_enum_v<T>)
            {
                using U = std::underlying_type_t<T>;
                U tmp;
                if (!GetValue(json, tmp))
                    return false;
                out = static_cast<T>(tmp);
                return true;
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                if (!json.IsNumber())
                    return false;
                out = static_cast<T>(json.GetDouble());
                return true;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_signed_v<T>)
                {
                    if constexpr (sizeof(T) <= 4)
                    {
                        if (!json.IsInt())
                            return false;
                        out = static_cast<T>(json.GetInt());
                        return true;
                    }
                    else
                    {
                        if (!json.IsInt64())
                            return false;
                        out = static_cast<T>(json.GetInt64());
                        return true;
                    }
                }
                else
                {
                    if constexpr (sizeof(T) <= 4)
                    {
                        if (!json.IsUint())
                            return false;
                        out = static_cast<T>(json.GetUint());
                        return true;
                    }
                    else
                    {
                        if (!json.IsUint64())
                            return false;
                        out = static_cast<T>(json.GetUint64());
                        return true;
                    }
                }
            }
            else
            {
                return false;
            }
        }

        /// @brief Writes a scalar value into a RapidJSON Value.
        /// @tparam T The C++ type to write from.
        /// @param [out] json The RapidJSON value to write to.
        /// @param [in] value The value to write.
        /// @param [in] allocator The document allocator (needed for string copies).
        template <typename T>
        void SetValue(rapidjson::Value& json, const T& value, rapidjson::Document::AllocatorType& allocator)
        {
            if constexpr (std::is_same_v<T, bool>)
            {
                json.SetBool(value);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                json.SetString(value.c_str(), static_cast<rapidjson::SizeType>(value.length()), allocator);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                using U = std::underlying_type_t<T>;
                SetValue(json, static_cast<U>(value), allocator);
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                json.SetDouble(static_cast<double>(value));
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_signed_v<T>)
                {
                    if constexpr (sizeof(T) <= 4)
                    {
                        json.SetInt(static_cast<int32_t>(value));
                    }
                    else
                    {
                        json.SetInt64(static_cast<int64_t>(value));
                    }
                }
                else
                {
                    if constexpr (sizeof(T) <= 4)
                    {
                        json.SetUint(static_cast<uint32_t>(value));
                    }
                    else
                    {
                        json.SetUint64(static_cast<uint64_t>(value));
                    }
                }
            }
        }

    }  // namespace detail

    /// @brief An object that can either deserialize JSON data or serializes it.
    ///
    /// The main purpose of this class is to avoid creating a serializer and a deserializer separately.
    /// This avoids duplicate code, but also errors, since the structure of the data is not encoded twice.
    ///
    /// This class also provides convenient ways to access properties that are several levels deep
    /// without needing to check if each level exists, which makes code cleaner.
    struct JsonMapper
    {
    public:
        /// @brief Creates a mapper that will serialize data to JSON.
        /// @return An object that will write to an internal JSON document.
        static JsonMapper Serialize();

        /// @brief Creates a mapper that will deserialize data from a JSON string.
        /// @param [in] data View over the JSON character data.
        /// @return An object that will read from the parsed JSON, or an invalid mapper on parse error.
        static JsonMapper Deserialize(std::span<const char> data);

        /// @brief Creates a mapper that will deserialize data from a JSON string.
        /// @param [in] json_string The JSON string to parse.
        /// @return An object that will read from the parsed JSON, or an invalid mapper on parse error.
        static JsonMapper Deserialize(const std::string& json_string);

        /// @brief Destructor.
        virtual ~JsonMapper() = default;

        /// @brief Returns true if this object is serializing data to JSON.
        bool IsSerializing() const;

        /// @brief Returns true if this object is deserializing data from JSON.
        bool IsDeserializing() const;

        /// @brief Returns true if this JsonMapper is valid.
        /// @return true if the JsonMapper is valid, false otherwise.
        bool IsValid() const;

        /// @brief Serializes the root JSON document to a string.
        /// @return The JSON string representation of the root document.
        std::string ToString() const;

        /// @brief Gets a const reference to the root RapidJSON value.
        /// @return The root RapidJSON value.
        const rapidjson::Value& Json() const;

        /// @brief Gets the JSON mapper object for the given key.
        /// @param [in] key The key of the JSON mapper to return.
        /// @return An object that will mapper that property.
        JsonMapper operator[](const std::string& key);

        /// @brief If IsSerializing() is true, this function writes a value; otherwise reads a value.
        ///
        /// This operator should only be used when it is desired that there is no default for a value or
        /// no sensible default value exists. The return value should be checked.
        /// @tparam T The type of value to read or write.
        /// @param [in,out] value If IsSerializing() is true, the value to write. Otherwise, the place to read the value to.
        /// @return true if the read / write operation was successful, false otherwise.
        template <typename T>
        bool operator()(T& value);

        /// @brief If IsSerializing() is true, this function writes a value; otherwise reads a value.
        /// @tparam T The type of value to read or write.
        /// @tparam D The type of the default value. This needs to be statically castable to T.
        /// @param [in,out] value If IsSerializing() is true, the value to write. Otherwise, the place to read the value to.
        /// @param [in] default_value If IsSerializing() is false and the property is not present on the JSON, the value to use.
        /// @return true if the read / write operation was successful, false otherwise. A read operation is considered
        ///              successful even if the default value is used.
        template <typename T, typename D>
        bool operator()(T& value, const D& default_value);

        /// @brief If IsSerializing() is true, this function writes a value; otherwise reads a value.
        /// @tparam T The type of value to read or write.
        /// @param [in,out] value If IsSerializing() is true, the value to write. Otherwise, the place to read the value to.
        /// @return true if the read / write operation was successful, false otherwise. A read operation is considered
        ///              successful even if the default value is used.
        template <typename T>
        bool operator()(std::optional<T>& value);

        /// @brief If IsSerializing() is true, writes an array of items to the JSON; otherwise reads an array from the JSON.
        ///
        /// This operator should only be used when it is desired that there is no default for a value or
        /// no sensible default value exists. The return value should be checked.
        /// @tparam T The type of element in the array. This must have a default constructor.
        /// @param [in,out] value If IsSerializing() is true, the items to write to the JSON. Otherwise, the place to read the items from the array into.
        /// @param [in] map_item A function to serialize / deserialize one item in the array.
        /// @return true if the read / write operation was successful, false otherwise.
        template <typename T>
        bool Arr(std::vector<T>& value, const std::function<bool(T&, JsonMapper)>& map_item);

        /// @brief If IsSerializing() is true, writes an array of items to the JSON; otherwise reads an array from the JSON.
        /// @tparam T The type of element in the array. This must have a default constructor.
        /// @param [in,out] value If IsSerializing() is true, the items to write to the JSON. Otherwise, the place to read the items from the array into.
        /// @param [in] map_item A function to serialize / deserialize one item in the array.
        /// @param [in] default_value The default value to use if the JSON is missing or an item fails to read.
        /// @return true if the read / write operation was successful, false otherwise.
        template <typename T>
        bool Arr(std::vector<T>& value, const std::function<bool(T&, JsonMapper)>& map_item, const std::vector<T>& default_value);

        /// @brief If IsSerializing() is true, writes a set as a JSON array; otherwise reads a JSON array into a set.
        /// @tparam T The type of element in the set. This must have a default constructor.
        /// @param [in,out] value If IsSerializing() is true, the items to write. Otherwise, the place to read items into.
        /// @param [in] map_item A function to serialize / deserialize one item.
        /// @return true if the read / write operation was successful, false otherwise.
        template <typename T>
        bool Set(std::unordered_set<T>& value, const std::function<bool(T&, JsonMapper)>& map_item);

        /// @brief If IsSerializing() is true, writes a set as a JSON array; otherwise reads a JSON array into a set.
        /// @tparam T The type of element in the set. This must have a default constructor.
        /// @param [in,out] value If IsSerializing() is true, the items to write. Otherwise, the place to read items into.
        /// @param [in] map_item A function to serialize / deserialize one item.
        /// @param [in] default_value The default value to use if the JSON is missing or an item fails to read.
        /// @return true if the read / write operation was successful, false otherwise.
        template <typename T>
        bool Set(std::unordered_set<T>& value, const std::function<bool(T&, JsonMapper)>& map_item, const std::unordered_set<T>& default_value);

    private:
        /// @brief Constructor to use when the json property is not valid.
        /// @param [in] is_serializing true if this object should be serializing, false otherwise.
        explicit JsonMapper(bool is_serializing);

        /// @brief Constructor to use when the json property is valid.
        /// @param [in,out] json The RapidJSON Value to operate on.
        /// @param [in] is_serializing true if this object should be serializing, false otherwise.
        /// @param [in,out] root_doc The root document (owns the allocator and all values).
        JsonMapper(rapidjson::Value* json, bool is_serializing, const std::shared_ptr<rapidjson::Document>& root_doc);

        /// @brief Maps a scalar or container value to/from JSON.
        template <typename T, typename D>
        bool Map(T& value, const D& default_value, bool use_default);

        /// @brief Maps an optional value to/from JSON.
        template <typename T>
        bool Map(std::optional<T>& value);

        /// @brief Maps an array/set of items to/from JSON using a callback.
        template <typename T, typename C>
        bool MapArr(C&                                         value,
                    const std::function<bool(T&, JsonMapper)>& map_item,
                    const C&                                   default_value,
                    bool                                       use_default,
                    const std::function<void(C&, T&)>&         append);

        /// @brief Gets a JsonMapper that will write to the given key.
        JsonMapper GetSerializing(const std::string& key);

        /// @brief Gets a JsonMapper that will read from the given key.
        JsonMapper GetDeserializing(const std::string& key);

        /// @brief Serializes a container (vector/set) of primitive elements to a JSON array.
        template <typename Container>
        bool SerializeContainer(const Container& value);

        /// @brief Deserializes a JSON array into a container (vector/set) of primitive elements.
        template <typename Container>
        bool DeserializeContainer(Container& value);

        bool                                 is_serializing_;  ///< true if this object is writing data to the json, false otherwise.
        bool                                 is_valid_;        ///< true if the json can be written to, false otherwise.
        rapidjson::Value*                    json_;            ///< The RapidJSON value to write to / read from.
        std::shared_ptr<rapidjson::Document> root_doc_;        ///< Root document (owns allocator). Shared to prevent premature deallocation.
    };

    // Regular property serialization
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T>
    bool JsonMapper::operator()(T& value)
    {
        return Map(value, value, false);
    }

    template <typename T, typename D>
    bool JsonMapper::operator()(T& value, const D& default_value)
    {
        return Map(value, default_value, true);
    }

    template <typename T>
    bool JsonMapper::operator()(std::optional<T>& value)
    {
        return Map(value);
    }

    template <typename Container>
    bool JsonMapper::SerializeContainer(const Container& value)
    {
        auto& allocator = root_doc_->GetAllocator();

        rapidjson::Value arr(rapidjson::kArrayType);
        for (const auto& elem : value)
        {
            rapidjson::Value elem_val;
            detail::SetValue(elem_val, elem, allocator);
            arr.PushBack(elem_val, allocator);
        }
        *json_ = arr;
        return true;
    }

    template <typename Container>
    bool JsonMapper::DeserializeContainer(Container& value)
    {
        using ElemType = Container::value_type;

        if (!json_->IsArray())
        {
            return false;
        }

        value.clear();
        for (rapidjson::SizeType i = 0; i < json_->Size(); ++i)
        {
            ElemType elem{};
            if (!detail::GetValue((*json_)[i], elem))
            {
                return false;
            }
            if constexpr (detail::is_std_vector_v<Container>)
            {
                value.push_back(std::move(elem));
            }
            else
            {
                value.emplace(std::move(elem));
            }
        }
        return true;
    }

    template <typename T, typename D>
    bool JsonMapper::Map(T& value, const D& default_value, bool use_default)
    {
        if (!is_valid_)
        {
            if (!is_serializing_ && use_default)
            {
                value = static_cast<T>(default_value);
                return true;
            }

            return false;
        }

        // Handle STL containers of primitive types (e.g. std::vector<std::string>, std::unordered_set<std::string>).
        if constexpr (detail::is_container_v<T>)
        {
            if (is_serializing_)
            {
                return SerializeContainer(value);
            }

            if (DeserializeContainer(value))
            {
                return true;
            }

            if (use_default)
            {
                value = static_cast<T>(default_value);
                return true;
            }
            return false;
        }
        else
        {
            // Scalar types (bool, integers, floats, strings, enums).
            if (is_serializing_)
            {
                detail::SetValue(*json_, value, root_doc_->GetAllocator());
                return true;
            }

            if (detail::GetValue(*json_, value))
            {
                return true;
            }

            if (use_default)
            {
                value = static_cast<T>(default_value);
                return true;
            }

            return false;
        }
    }

    template <typename T>
    bool JsonMapper::Map(std::optional<T>& value)
    {
        if (!is_valid_)
        {
            if (!is_serializing_)
            {
                value = std::nullopt;
                return true;
            }

            return false;
        }

        if (is_serializing_)
        {
            if (value.has_value())
            {
                detail::SetValue(*json_, value.value(), root_doc_->GetAllocator());
                return true;
            }
            return false;
        }

        T val{};
        if (detail::GetValue(*json_, val))
        {
            value = val;
            return true;
        }

        value = std::nullopt;
        return false;
    }

    // Array serialization
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T>
    bool JsonMapper::Arr(std::vector<T>& value, const std::function<bool(T&, JsonMapper)>& map_item)
    {
        return MapArr<T, std::vector<T>>(value, map_item, value, false, [](std::vector<T>& v, T& item) { v.emplace_back(item); });
    }

    template <typename T>
    bool JsonMapper::Arr(std::vector<T>& value, const std::function<bool(T&, JsonMapper)>& map_item, const std::vector<T>& default_value)
    {
        return MapArr<T, std::vector<T>>(value, map_item, default_value, true, [](std::vector<T>& v, T& item) { v.emplace_back(item); });
    }

    template <typename T>
    bool JsonMapper::Set(std::unordered_set<T>& value, const std::function<bool(T&, JsonMapper)>& map_item)
    {
        return MapArr<T, std::unordered_set<T>>(value, map_item, value, false, [](std::unordered_set<T>& v, T& item) { v.emplace(item); });
    }

    template <typename T>
    bool JsonMapper::Set(std::unordered_set<T>& value, const std::function<bool(T&, JsonMapper)>& map_item, const std::unordered_set<T>& default_value)
    {
        return MapArr<T, std::unordered_set<T>>(value, map_item, default_value, true, [](std::unordered_set<T>& v, T& item) { v.emplace(item); });
    }

    template <typename T, typename C>
    bool JsonMapper::MapArr(C&                                         value,
                            const std::function<bool(T&, JsonMapper)>& map_item,
                            const C&                                   default_value,
                            bool                                       use_default,
                            const std::function<void(C&, T&)>&         append)
    {
        if (!is_valid_)
        {
            if (!is_serializing_)
            {
                if (use_default)
                {
                    value = default_value;
                    return true;
                }

                value.clear();
            }

            return false;
        }

        if (is_serializing_)
        {
            auto& allocator = root_doc_->GetAllocator();

            rapidjson::Value array(rapidjson::kArrayType);
            for (const T& item : value)
            {
                rapidjson::Value item_json(rapidjson::kObjectType);

                // We have to use a const cast here because std::unordered_set doesn't support non-const iteration. This is fine because no data is going
                // to be modified.
                if (!map_item(const_cast<T&>(item), JsonMapper(&item_json, true, root_doc_)))
                {
                    return false;
                }

                array.PushBack(item_json, allocator);
            }

            *json_ = array;
            return true;
        }

        value.clear();
        if (!json_->IsArray())
        {
            if (use_default)
            {
                value = default_value;
                return true;
            }

            return false;
        }

        for (rapidjson::SizeType i = 0; i < json_->Size(); ++i)
        {
            T item{};

            if (!map_item(item, JsonMapper(&(*json_)[i], false, root_doc_)))
            {
                if (use_default)
                {
                    value = default_value;
                    return true;
                }

                value.clear();
                return false;
            }

            append(value, item);
        }

        return true;
    }

};  // namespace devtrace

#endif
