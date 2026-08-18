// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for JSON mapper.

#include "json/json_mapper.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace devtrace
{
    JsonMapper JsonMapper::Serialize()
    {
        auto doc = std::make_shared<rapidjson::Document>();
        doc->SetObject();
        return {static_cast<rapidjson::Value*>(doc.get()), true, doc};
    }

    JsonMapper JsonMapper::Deserialize(std::span<const char> data)
    {
        auto doc = std::make_shared<rapidjson::Document>();
        doc->Parse(data.data(), data.size());
        if (doc->HasParseError())
        {
            return JsonMapper(false);
        }
        return {static_cast<rapidjson::Value*>(doc.get()), false, doc};
    }

    JsonMapper JsonMapper::Deserialize(const std::string& json_string)
    {
        return Deserialize(std::span<const char>{json_string.data(), json_string.size()});
    }

    std::string JsonMapper::ToString() const
    {
        if (!root_doc_)
        {
            return "{}";
        }

        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        root_doc_->Accept(writer);
        return {buffer.GetString(), buffer.GetSize()};
    }

    const rapidjson::Value& JsonMapper::Json() const
    {
        DEV_TRACE_ASSERT(root_doc_ != nullptr);
        return *root_doc_;
    }

    JsonMapper::JsonMapper(bool is_serializing)
        : is_serializing_(is_serializing)
        , is_valid_(false)
        , json_(nullptr)
        , root_doc_(nullptr)
    {
    }

    JsonMapper::JsonMapper(rapidjson::Value* json, bool is_serializing, const std::shared_ptr<rapidjson::Document>& root_doc)
        : is_serializing_(is_serializing)
        , is_valid_(true)
        , json_(json)
        , root_doc_(root_doc)
    {
    }

    bool JsonMapper::IsSerializing() const
    {
        return is_serializing_;
    }

    bool JsonMapper::IsDeserializing() const
    {
        return !is_serializing_;
    }

    bool JsonMapper::IsValid() const
    {
        return is_valid_;
    }

    JsonMapper JsonMapper::operator[](const std::string& key)
    {
        if (!is_valid_)
        {
            return *this;
        }

        if (is_serializing_)
        {
            return GetSerializing(key);
        }

        return GetDeserializing(key);
    }

    JsonMapper JsonMapper::GetSerializing(const std::string& key)
    {
        auto& allocator = root_doc_->GetAllocator();

        if (json_->IsNull())
        {
            json_->SetObject();
        }

        if (!json_->IsObject())
        {
            return JsonMapper(true);
        }

        if (!json_->HasMember(key.c_str()))
        {
            rapidjson::Value key_val;
            key_val.SetString(key.c_str(), static_cast<rapidjson::SizeType>(key.length()), allocator);
            json_->AddMember(key_val, rapidjson::Value(rapidjson::kObjectType), allocator);
        }

        return {&(*json_)[key.c_str()], true, root_doc_};
    }

    JsonMapper JsonMapper::GetDeserializing(const std::string& key)
    {
        if (json_->IsObject() && json_->HasMember(key.c_str()))
        {
            return {&(*json_)[key.c_str()], false, root_doc_};
        }

        return JsonMapper(false);
    }

};  // namespace devtrace
