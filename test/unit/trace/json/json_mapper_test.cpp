// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the JSON mapper.

#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <json/json_mapper.h>

namespace devtrace
{
    // Example tests
    // These tests show the power of JsonMapper and how it can be used to quickly write a combined serializer / deserializer
    // for reasonably complex data. There's no need to check if properties exist, no need to write for loops for arrays
    // and no need to write separate code for serialization and deserialization.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Data structure
    struct NestedObject
    {
        std::string string = "";
        uint32_t    uint   = 0;

        NestedObject() = default;
        NestedObject(std::string s, uint32_t i)
            : string(s)
            , uint(i)
        {
        }
    };

    struct ArrayItemIndex
    {
        int index        = 0;
        ArrayItemIndex() = default;
        ArrayItemIndex(int i)
            : index(i)
        {
        }
    };

    struct ArrayItem
    {
        bool           is_cool = false;
        ArrayItemIndex pos     = {};

        ArrayItem() = default;
        ArrayItem(bool cool, ArrayItemIndex index)
            : is_cool(cool)
            , pos(index)
        {
        }
    };

    struct TopLevelObject
    {
        std::vector<ArrayItem> items   = {};
        std::vector<uint8_t>   indices = {};  // This can actually be serialized / deserialized without using Arr().
        NestedObject           nested;
        std::string            name      = "";
        uint8_t                version   = 0;
        uint64_t               timestamp = 0;
    };

    // Serialization / deserialization functions
    static bool SerializeArrayItem(ArrayItem& item, JsonMapper serializer)
    {
        return serializer["is_cool"](item.is_cool, true) && serializer["pos"]["index"](item.pos.index, -1);
    }

    static bool SerializeNested(NestedObject& obj, JsonMapper serializer)
    {
        return serializer["string"](obj.string, "default") && serializer["uint"](obj.uint, 32);
    }

    static bool Serialize(TopLevelObject& obj, JsonMapper& serializer)
    {
        return serializer["name"](obj.name) && serializer["version"](obj.version, 0) && serializer["timestamp"](obj.timestamp, 0) &&
               SerializeNested(obj.nested, serializer["nested"]) &&
               serializer["items"].Arr<ArrayItem>(obj.items, SerializeArrayItem, {{true, {0}}, {false, {1}}}) &&
               serializer["indices"](obj.indices, std::vector<uint8_t>{});
    }

    TEST(JsonMapperTests, TestSerializesAndDeserializes)
    {
        TopLevelObject obj;
        obj.name      = "TestSerializesAndDeserializes";
        obj.version   = 1;
        obj.timestamp = 1660540036;
        obj.indices   = {39, 3, 0, 90};
        obj.items     = {{true, {0}}, {true, {1}}, {false, {2}}, {true, {3}}, {false, {4}}};
        obj.nested    = {"cool nested string", 192387};

        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(Serialize(obj, serializer));

        TopLevelObject deserialized;
        std::string    result_str = serializer.ToString();

        JsonMapper deserializer = JsonMapper::Deserialize(result_str);
        EXPECT_TRUE(Serialize(deserialized, deserializer));

        ASSERT_EQ(obj.items.size(), deserialized.items.size());
        for (int i = 0; i < static_cast<int>(obj.items.size()); i++)
        {
            EXPECT_EQ(obj.items[i].is_cool, deserialized.items[i].is_cool);
            EXPECT_EQ(obj.items[i].pos.index, deserialized.items[i].pos.index);
        }

        ASSERT_EQ(obj.indices.size(), deserialized.indices.size());
        for (int i = 0; i < static_cast<int>(obj.indices.size()); i++)
        {
            EXPECT_EQ(obj.indices[i], deserialized.indices[i]);
        }

        EXPECT_EQ(obj.name, deserialized.name);
        EXPECT_EQ(obj.version, deserialized.version);
        EXPECT_EQ(obj.nested.string, deserialized.nested.string);
        EXPECT_EQ(obj.nested.uint, deserialized.nested.uint);
    }

    TEST(JsonMapperTests, TestDeserializesWithDefaults)
    {
        std::string json_str = R"({"name": "TestDeserializesWithDefaults"})";

        TopLevelObject deserialized;
        JsonMapper     deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(Serialize(deserialized, deserializer));

        ASSERT_EQ(2, deserialized.items.size());
        EXPECT_TRUE(deserialized.items[0].is_cool);
        EXPECT_EQ(0, deserialized.items[0].pos.index);

        EXPECT_FALSE(deserialized.items[1].is_cool);
        EXPECT_EQ(1, deserialized.items[1].pos.index);

        EXPECT_EQ("TestDeserializesWithDefaults", deserialized.name);
        EXPECT_EQ(0, deserialized.version);
        EXPECT_EQ("default", deserialized.nested.string);
        EXPECT_EQ(32, deserialized.nested.uint);
    }

    TEST(JsonMapperTests, TestDeserializesFailsWithoutRequiredProperty)
    {
        std::string json_str = R"({})";

        TopLevelObject deserialized;
        JsonMapper     deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(Serialize(deserialized, deserializer));
    }

    // Simple property writes
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestWritesBoolProperty)
    {
        bool       value      = true;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["bool_val"](value));

        EXPECT_TRUE(serializer.Json()["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestWritesCharProperty)
    {
        char       value      = 'F';
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["char_val"](value));

        EXPECT_EQ('F', static_cast<char>(serializer.Json()["char_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesShortProperty)
    {
        short      value      = 153;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["short_val"](value));

        EXPECT_EQ(153, static_cast<short>(serializer.Json()["short_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesIntProperty)
    {
        int        value      = 37819;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int_val"](value));

        EXPECT_EQ(37819, serializer.Json()["int_val"].GetInt());
    }

    TEST(JsonMapperTests, TestWritesLongProperty)
    {
        long       value      = 378191335;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["long_val"](value));

        EXPECT_EQ(378191335, static_cast<long>(serializer.Json()["long_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesLongLongProperty)
    {
        long long  value      = 3781913351928;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["long_long_val"](value));

        EXPECT_EQ(3781913351928, serializer.Json()["long_long_val"].GetInt64());
    }

    TEST(JsonMapperTests, TestWritesInt8Property)
    {
        int8_t     value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int8_val"](value));

        EXPECT_EQ(123, static_cast<int8_t>(serializer.Json()["int8_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesInt16Property)
    {
        int16_t    value      = 12143;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int16_val"](value));

        EXPECT_EQ(12143, static_cast<int16_t>(serializer.Json()["int16_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesInt32Property)
    {
        int32_t    value      = 121414433;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int32_val"](value));

        EXPECT_EQ(121414433, serializer.Json()["int32_val"].GetInt());
    }

    TEST(JsonMapperTests, TestWritesInt64Property)
    {
        int64_t    value      = 1214141444445533;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int64_val"](value));

        EXPECT_EQ(1214141444445533, serializer.Json()["int64_val"].GetInt64());
    }

    TEST(JsonMapperTests, TestWritesUint8Property)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint8_val"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["uint8_val"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesUint16Property)
    {
        uint16_t   value      = 12143;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint16_val"](value));

        EXPECT_EQ(12143, static_cast<uint16_t>(serializer.Json()["uint16_val"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesUint32Property)
    {
        uint32_t   value      = 121414433;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint32_val"](value));

        EXPECT_EQ(121414433, serializer.Json()["uint32_val"].GetUint());
    }

    TEST(JsonMapperTests, TestWritesUint64Property)
    {
        uint64_t   value      = 1214141444445533;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint64_val"](value));

        EXPECT_EQ(1214141444445533, serializer.Json()["uint64_val"].GetUint64());
    }

    TEST(JsonMapperTests, TestWritesFloatProperty)
    {
        float      value      = 438902.1234f;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["float_val"](value));

        EXPECT_FLOAT_EQ(438902.1234f, static_cast<float>(serializer.Json()["float_val"].GetDouble()));
    }

    TEST(JsonMapperTests, TestWritesDoubleProperty)
    {
        double     value      = 4389102.1234123123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["double_val"](value));

        EXPECT_DOUBLE_EQ(4389102.1234123123, serializer.Json()["double_val"].GetDouble());
    }

    TEST(JsonMapperTests, TestWritesStringProperty)
    {
        std::string value      = "hello world";
        JsonMapper  serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["str_val"](value));

        EXPECT_EQ("hello world", std::string(serializer.Json()["str_val"].GetString()));
    }

    // Simple property writes with default
    // The default argument doesn't do anything for writes, but these tests exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestWritesBoolPropertyWithDefault)
    {
        bool       value      = true;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["bool_val"](value, false));

        EXPECT_TRUE(serializer.Json()["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestWritesCharPropertyWithDefault)
    {
        char       value      = 'F';
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["char_val"](value, '\0'));

        EXPECT_EQ('F', static_cast<char>(serializer.Json()["char_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesShortPropertyWithDefault)
    {
        short      value      = 153;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["short_val"](value, 0));

        EXPECT_EQ(153, static_cast<short>(serializer.Json()["short_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesIntPropertyWithDefault)
    {
        int        value      = 37819;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int_val"](value, 0));

        EXPECT_EQ(37819, serializer.Json()["int_val"].GetInt());
    }

    TEST(JsonMapperTests, TestWritesLongPropertyWithDefault)
    {
        long       value      = 378191335;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["long_val"](value, 0));

        EXPECT_EQ(378191335, static_cast<long>(serializer.Json()["long_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesLongLongPropertyWithDefault)
    {
        long long  value      = 3781913351928;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["long_long_val"](value, 0));

        EXPECT_EQ(3781913351928, serializer.Json()["long_long_val"].GetInt64());
    }

    TEST(JsonMapperTests, TestWritesInt8PropertyWithDefault)
    {
        int8_t     value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int8_val"](value, 0));

        EXPECT_EQ(123, static_cast<int8_t>(serializer.Json()["int8_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesInt16PropertyWithDefault)
    {
        int16_t    value      = 12143;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int16_val"](value, 0));

        EXPECT_EQ(12143, static_cast<int16_t>(serializer.Json()["int16_val"].GetInt()));
    }

    TEST(JsonMapperTests, TestWritesInt32PropertyWithDefault)
    {
        int32_t    value      = 121414433;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int32_val"](value, 0));

        EXPECT_EQ(121414433, serializer.Json()["int32_val"].GetInt());
    }

    TEST(JsonMapperTests, TestWritesInt64PropertyWithDefault)
    {
        int64_t    value      = 1214141444445533;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["int64_val"](value, 0));

        EXPECT_EQ(1214141444445533, serializer.Json()["int64_val"].GetInt64());
    }

    TEST(JsonMapperTests, TestWritesUint8PropertyWithDefault)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint8_val"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["uint8_val"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesUint16PropertyWithDefault)
    {
        uint16_t   value      = 12143;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint16_val"](value, 0));

        EXPECT_EQ(12143, static_cast<uint16_t>(serializer.Json()["uint16_val"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesUint32PropertyWithDefault)
    {
        uint32_t   value      = 121414433;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint32_val"](value, 0));

        EXPECT_EQ(121414433, serializer.Json()["uint32_val"].GetUint());
    }

    TEST(JsonMapperTests, TestWritesUint64PropertyWithDefault)
    {
        uint64_t   value      = 1214141444445533;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["uint64_val"](value, 0));

        EXPECT_EQ(1214141444445533, serializer.Json()["uint64_val"].GetUint64());
    }

    TEST(JsonMapperTests, TestWritesFloatPropertyWithDefault)
    {
        float      value      = 438902.1234f;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["float_val"](value, 0.0));

        EXPECT_FLOAT_EQ(438902.1234f, static_cast<float>(serializer.Json()["float_val"].GetDouble()));
    }

    TEST(JsonMapperTests, TestWritesDoublePropertyWithDefault)
    {
        double     value      = 4389102.1234123123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["double_val"](value, 0.0));

        EXPECT_DOUBLE_EQ(4389102.1234123123, serializer.Json()["double_val"].GetDouble());
    }

    TEST(JsonMapperTests, TestWritesStringPropertyWithDefault)
    {
        std::string value      = "hello world";
        JsonMapper  serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["str_val"](value, ""));

        EXPECT_EQ("hello world", std::string(serializer.Json()["str_val"].GetString()));
    }

    // Simple property reads
    // None of these tests actually leverage the default, they exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadsBoolProperty)
    {
        std::string json_str = R"({"bool_val": true})";

        bool       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["bool_val"](value));

        EXPECT_TRUE(value);
    }

    TEST(JsonMapperTests, TestReadsCharProperty)
    {
        std::string json_str = R"({"char_val": 87})";

        char       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["char_val"](value));

        EXPECT_EQ('W', value);
    }

    TEST(JsonMapperTests, TestReadsIntProperty)
    {
        std::string json_str = R"({"int_val": 135435})";

        int        value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int_val"](value));

        EXPECT_EQ(135435, value);
    }

    TEST(JsonMapperTests, TestReadsLongProperty)
    {
        std::string json_str = R"({"long_val": 1353333435})";

        long       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_val"](value));

        EXPECT_EQ(1353333435, value);
    }

    TEST(JsonMapperTests, TestReadsLongLongProperty)
    {
        std::string json_str = R"({"long_long_val": 1353333433335})";

        long long  value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_long_val"](value));

        EXPECT_EQ(1353333433335, value);
    }

    TEST(JsonMapperTests, TestReadsInt8Property)
    {
        std::string json_str = R"({"int8_val": 123})";

        int8_t     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int8_val"](value));

        EXPECT_EQ(123, value);
    }

    TEST(JsonMapperTests, TestReadsInt16Property)
    {
        std::string json_str = R"({"int16_val": 3323})";

        int16_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int16_val"](value));

        EXPECT_EQ(3323, value);
    }

    TEST(JsonMapperTests, TestReadsInt32Property)
    {
        std::string json_str = R"({"int32_val": 333125553})";

        int32_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int32_val"](value));

        EXPECT_EQ(333125553, value);
    }

    TEST(JsonMapperTests, TestReadsInt64Property)
    {
        std::string json_str = R"({"int64_val": 333125549941153})";

        int64_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int64_val"](value));

        EXPECT_EQ(333125549941153, value);
    }

    TEST(JsonMapperTests, TestReadsUint8Property)
    {
        std::string json_str = R"({"uint8_val": 123})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint8_val"](value));

        EXPECT_EQ(123, value);
    }

    TEST(JsonMapperTests, TestReadsUint16Property)
    {
        std::string json_str = R"({"uint16_val": 3313})";

        uint16_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint16_val"](value));

        EXPECT_EQ(3313, value);
    }

    TEST(JsonMapperTests, TestReadsUint32Property)
    {
        std::string json_str = R"({"uint32_val": 333125553})";

        uint32_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint32_val"](value));

        EXPECT_EQ(333125553, value);
    }

    TEST(JsonMapperTests, TestReadsUint64Property)
    {
        std::string json_str = R"({"uint64_val": 333125549941153})";

        uint64_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint64_val"](value));

        EXPECT_EQ(333125549941153, value);
    }

    TEST(JsonMapperTests, TestReadsFloatProperty)
    {
        std::string json_str = R"({"float_val": 3331255.5})";

        float      value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["float_val"](value));

        EXPECT_FLOAT_EQ(3331255.49941153f, value);
    }

    TEST(JsonMapperTests, TestReadsDoubleProperty)
    {
        std::string json_str = R"({"double_val": 3331255.499411534412313})";

        double     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["double_val"](value));

        EXPECT_DOUBLE_EQ(3331255.499411534412313, value);
    }

    TEST(JsonMapperTests, TestReadsStringProperty)
    {
        std::string json_str = R"({"str_val": "hello sunshine"})";

        std::string value;
        JsonMapper  deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["str_val"](value));

        EXPECT_EQ("hello sunshine", value);
    }

    // Simple property reads with default
    // None of these tests actually leverage the default, they exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadsBoolPropertyWithDefault)
    {
        std::string json_str = R"({"bool_val": true})";

        bool       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["bool_val"](value, false));

        EXPECT_TRUE(value);
    }

    TEST(JsonMapperTests, TestReadsCharPropertyWithDefault)
    {
        std::string json_str = R"({"char_val": 87})";

        char       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["char_val"](value, '\0'));

        EXPECT_EQ('W', value);
    }

    TEST(JsonMapperTests, TestReadsIntPropertyWithDefault)
    {
        std::string json_str = R"({"int_val": 135435})";

        int        value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int_val"](value, 0));

        EXPECT_EQ(135435, value);
    }

    TEST(JsonMapperTests, TestReadsLongPropertyWithDefault)
    {
        std::string json_str = R"({"long_val": 1353333435})";

        long       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_val"](value, 0));

        EXPECT_EQ(1353333435, value);
    }

    TEST(JsonMapperTests, TestReadsLongLongPropertyWithDefault)
    {
        std::string json_str = R"({"long_long_val": 1353333433335})";

        long long  value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_long_val"](value, 0));

        EXPECT_EQ(1353333433335, value);
    }

    TEST(JsonMapperTests, TestReadsInt8PropertyWithDefault)
    {
        std::string json_str = R"({"int8_val": 123})";

        int8_t     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int8_val"](value, 0));

        EXPECT_EQ(123, value);
    }

    TEST(JsonMapperTests, TestReadsInt16PropertyWithDefault)
    {
        std::string json_str = R"({"int16_val": 3323})";

        int16_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int16_val"](value, 0));

        EXPECT_EQ(3323, value);
    }

    TEST(JsonMapperTests, TestReadsInt32PropertyWithDefault)
    {
        std::string json_str = R"({"int32_val": 333125553})";

        int32_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int32_val"](value, 0));

        EXPECT_EQ(333125553, value);
    }

    TEST(JsonMapperTests, TestReadsInt64PropertyWithDefault)
    {
        std::string json_str = R"({"int64_val": 333125549941153})";

        int64_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int64_val"](value, 0));

        EXPECT_EQ(333125549941153, value);
    }

    TEST(JsonMapperTests, TestReadsUint8PropertyWithDefault)
    {
        std::string json_str = R"({"uint8_val": 123})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint8_val"](value, 0));

        EXPECT_EQ(123, value);
    }

    TEST(JsonMapperTests, TestReadsUint16PropertyWithDefault)
    {
        std::string json_str = R"({"uint16_val": 3313})";

        uint16_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint16_val"](value, 0));

        EXPECT_EQ(3313, value);
    }

    TEST(JsonMapperTests, TestReadsUint32PropertyWithDefault)
    {
        std::string json_str = R"({"uint32_val": 333125553})";

        uint32_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint32_val"](value, 0));

        EXPECT_EQ(333125553, value);
    }

    TEST(JsonMapperTests, TestReadsUint64PropertyWithDefault)
    {
        std::string json_str = R"({"uint64_val": 333125549941153})";

        uint64_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint64_val"](value, 0));

        EXPECT_EQ(333125549941153, value);
    }

    TEST(JsonMapperTests, TestReadsFloatPropertyWithDefault)
    {
        std::string json_str = R"({"float_val": 3331255.5})";

        float      value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["float_val"](value, 0.0));

        EXPECT_FLOAT_EQ(3331255.49941153f, value);
    }

    TEST(JsonMapperTests, TestReadsDoublePropertyWithDefault)
    {
        std::string json_str = R"({"double_val": 3331255.499411534412313})";

        double     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["double_val"](value, 0.0));

        EXPECT_DOUBLE_EQ(3331255.499411534412313, value);
    }

    TEST(JsonMapperTests, TestReadsStringPropertyWithDefault)
    {
        std::string json_str = R"({"str_val": "hello sunshine"})";

        std::string value;
        JsonMapper  deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["str_val"](value, ""));

        EXPECT_EQ("hello sunshine", value);
    }

    // Missing property reads without default
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadsBoolPropertyMissing)
    {
        std::string json_str = R"({})";

        bool       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["bool_val"](value));
    }

    TEST(JsonMapperTests, TestReadsCharPropertyMissing)
    {
        std::string json_str = R"({})";

        char       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["char_val"](value));
    }

    TEST(JsonMapperTests, TestReadsIntPropertyMissing)
    {
        std::string json_str = R"({})";

        int        value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["int_val"](value));
    }

    TEST(JsonMapperTests, TestReadsLongPropertyMissing)
    {
        std::string json_str = R"({})";

        long       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["long_val"](value));
    }

    TEST(JsonMapperTests, TestReadsLongLongPropertyMissing)
    {
        std::string json_str = R"({})";

        long long  value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["long_long_val"](value));
    }

    TEST(JsonMapperTests, TestReadsInt8PropertyMissing)
    {
        std::string json_str = R"({})";

        int8_t     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["int8_val"](value));
    }

    TEST(JsonMapperTests, TestReadsInt16PropertyMissing)
    {
        std::string json_str = R"({})";

        int16_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["int16_val"](value));
    }

    TEST(JsonMapperTests, TestReadsInt32PropertyMissing)
    {
        std::string json_str = R"({})";

        int32_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["int32_val"](value));
    }

    TEST(JsonMapperTests, TestReadsInt64PropertyMissing)
    {
        std::string json_str = R"({})";

        int64_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["int64_val"](value));
    }

    TEST(JsonMapperTests, TestReadsUint8PropertyMissing)
    {
        std::string json_str = R"({})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["uint8_val"](value));
    }

    TEST(JsonMapperTests, TestReadsUint16PropertyMissing)
    {
        std::string json_str = R"({})";

        uint16_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["uint16_val"](value));
    }

    TEST(JsonMapperTests, TestReadsUint32PropertyMissing)
    {
        std::string json_str = R"({})";

        uint32_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["uint32_val"](value));
    }

    TEST(JsonMapperTests, TestReadsUint64PropertyMissing)
    {
        std::string json_str = R"({})";

        uint64_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["uint64_val"](value));
    }

    TEST(JsonMapperTests, TestReadsFloatPropertyMissing)
    {
        std::string json_str = R"({})";

        float      value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["float_val"](value));
    }

    TEST(JsonMapperTests, TestReadsDoublePropertyMissing)
    {
        std::string json_str = R"({})";

        double     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["double_val"](value));
    }

    TEST(JsonMapperTests, TestReadsStringPropertyMissing)
    {
        std::string json_str = R"({})";

        std::string value;
        JsonMapper  deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["str_val"](value));
    }

    // Missing property reads with default
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadsBoolPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        bool       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["bool_val"](value, true));

        EXPECT_TRUE(value);
    }

    TEST(JsonMapperTests, TestReadsCharPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        char       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["char_val"](value, 'Q'));

        EXPECT_EQ('Q', value);
    }

    TEST(JsonMapperTests, TestReadsIntPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        int        value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int_val"](value, 21));

        EXPECT_EQ(21, value);
    }

    TEST(JsonMapperTests, TestReadsLongPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        long       value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_val"](value, 582398));

        EXPECT_EQ(582398, value);
    }

    TEST(JsonMapperTests, TestReadsLongLongPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        long long  value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["long_long_val"](value, 983758954958));

        EXPECT_EQ(983758954958, value);
    }

    TEST(JsonMapperTests, TestReadsInt8PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        int8_t     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int8_val"](value, 43));

        EXPECT_EQ(43, value);
    }

    TEST(JsonMapperTests, TestReadsInt16PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        int16_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int16_val"](value, 1214));

        EXPECT_EQ(1214, value);
    }

    TEST(JsonMapperTests, TestReadsInt32PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        int32_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int32_val"](value, 54189));

        EXPECT_EQ(54189, value);
    }

    TEST(JsonMapperTests, TestReadsInt64PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        int64_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["int64_val"](value, 89495642554431));

        EXPECT_EQ(89495642554431, value);
    }

    TEST(JsonMapperTests, TestReadsUint8PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint8_val"](value, 42));

        EXPECT_EQ(42, value);
    }

    TEST(JsonMapperTests, TestReadsUint16PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        uint16_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint16_val"](value, 54843));

        EXPECT_EQ(54843, value);
    }

    TEST(JsonMapperTests, TestReadsUint32PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        uint32_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint32_val"](value, 49648644));

        EXPECT_EQ(49648644, value);
    }

    TEST(JsonMapperTests, TestReadsUint64PropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        uint64_t   value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["uint64_val"](value, 5489574392344));

        EXPECT_EQ(5489574392344, value);
    }

    TEST(JsonMapperTests, TestReadsFloatPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        float      value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["float_val"](value, 21.42390f));

        EXPECT_FLOAT_EQ(21.42390f, value);
    }

    TEST(JsonMapperTests, TestReadsDoublePropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        double     value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["double_val"](value, 5948.0324874373));

        EXPECT_DOUBLE_EQ(5948.0324874373, value);
    }

    TEST(JsonMapperTests, TestReadsStringPropertyMissingWithDefault)
    {
        std::string json_str = R"({})";

        std::string value;
        JsonMapper  deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["str_val"](value, "goodnight"));

        EXPECT_EQ("goodnight", value);
    }

    // Special writes
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestWritesOnExistingNestedObject)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesNestedObject)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesDeep)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["a"]["b"]["c"]["value"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["a"]["b"]["c"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestNestedWritesWontReplaceValue)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        bool       bool_val   = true;
        serializer["bool_val"](bool_val);

        uint8_t value = 123;
        EXPECT_FALSE(serializer["bool_val"]["deeper"](value));

        EXPECT_TRUE(serializer.Json()["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestNestedWritesWontReplaceValueTwoDeep)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        bool       bool_val   = true;
        serializer["value"]["bool_val"](bool_val);

        uint8_t value = 123;
        EXPECT_FALSE(serializer["value"]["bool_val"]["deeper"](value));

        EXPECT_TRUE(serializer.Json()["value"]["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestReplacesAnObject)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        uint8_t    dummy      = 0;
        serializer["value"]["inner"](dummy);

        uint8_t value = 123;
        EXPECT_TRUE(serializer["value"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestReplacesAValue)
    {
        JsonMapper  serializer = JsonMapper::Serialize();
        std::string hello      = "hello";
        serializer["value"](hello);

        uint8_t value = 123;
        EXPECT_TRUE(serializer["value"](value));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestOverwritesExistingProperty)
    {
        uint8_t value0 = 123;
        uint8_t value1 = 99;

        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"]["deep"](value0));
        EXPECT_TRUE(serializer["obj"]["value"](value1));

        EXPECT_EQ(99, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    // Special writes with default
    // The default argument doesn't do anything for writes, but these tests exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestWritesOnExistingNestedObjectWithDefault)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesNestedObjectWithDefault)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestWritesDeepWithDefault)
    {
        uint8_t    value      = 123;
        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["a"]["b"]["c"]["value"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["obj"]["a"]["b"]["c"]["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestNestedWritesWontReplaceValueWithDefault)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        bool       bool_val   = true;
        serializer["bool_val"](bool_val);

        uint8_t value = 123;
        EXPECT_FALSE(serializer["bool_val"]["deeper"](value, 0));

        EXPECT_TRUE(serializer.Json()["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestNestedWritesWontReplaceValueTwoDeepWithDefault)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        bool       bool_val   = true;
        serializer["value"]["bool_val"](bool_val);

        uint8_t value = 123;
        EXPECT_FALSE(serializer["value"]["bool_val"]["deeper"](value, 0));

        EXPECT_TRUE(serializer.Json()["value"]["bool_val"].GetBool());
    }

    TEST(JsonMapperTests, TestReplacesAnObjectWithDefault)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        uint8_t    dummy      = 0;
        serializer["value"]["inner"](dummy);

        uint8_t value = 123;
        EXPECT_TRUE(serializer["value"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestReplacesAValueWithDefault)
    {
        JsonMapper  serializer = JsonMapper::Serialize();
        std::string hello      = "hello";
        serializer["value"](hello);

        uint8_t value = 123;
        EXPECT_TRUE(serializer["value"](value, 0));

        EXPECT_EQ(123, static_cast<uint8_t>(serializer.Json()["value"].GetUint()));
    }

    TEST(JsonMapperTests, TestOverwritesExistingPropertyWithDefault)
    {
        uint8_t value0 = 123;
        uint8_t value1 = 99;

        JsonMapper serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["obj"]["value"]["deep"](value0, 0));
        EXPECT_TRUE(serializer["obj"]["value"](value1, 0));

        EXPECT_EQ(99, static_cast<uint8_t>(serializer.Json()["obj"]["value"].GetUint()));
    }

    // Special reads
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadNestedMissingProperty)
    {
        std::string json_str = R"({"value": "hello"})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["value"]["integer"](value));
    }

    TEST(JsonMapperTests, TestReadPropertyWrongType)
    {
        std::string json_str = R"({"value": "hello"})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["value"](value));
    }

    TEST(JsonMapperTests, TestReadPropertyNestedWrongType)
    {
        std::string json_str = R"({"top": {"value": "hello"}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_FALSE(deserializer["top"]["value"](value));
    }

    TEST(JsonMapperTests, TestReadPropertyNested)
    {
        std::string json_str = R"({"top": {"value": 35}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["top"]["value"](value));

        EXPECT_EQ(35, value);
    }

    TEST(JsonMapperTests, TestReadPropertyNestedDeep)
    {
        std::string json_str = R"({"top": {"a": {"b": {"c": {"d": {"e": {"value": 35}}}}}}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["top"]["a"]["b"]["c"]["d"]["e"]["value"](value));

        EXPECT_EQ(35, value);
    }

    // Special reads with defaults
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestReadNestedMissingPropertyWithDefault)
    {
        std::string json_str = R"({"value": "hello"})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["value"]["integer"](value, 92));

        EXPECT_EQ(92, value);
    }

    TEST(JsonMapperTests, TestReadPropertyWrongTypeWithDefault)
    {
        std::string json_str = R"({"value": "hello"})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["value"](value, 85));

        EXPECT_EQ(85, value);
    }

    TEST(JsonMapperTests, TestReadPropertyNestedWrongTypeWithDefault)
    {
        std::string json_str = R"({"top": {"value": "hello"}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["top"]["value"](value, 21));

        EXPECT_EQ(21, value);
    }

    TEST(JsonMapperTests, TestReadPropertyNestedWithDefault)
    {
        std::string json_str = R"({"top": {"value": 35}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["top"]["value"](value, 0));

        EXPECT_EQ(35, value);
    }

    TEST(JsonMapperTests, TestReadPropertyNestedDeepWithDefault)
    {
        std::string json_str = R"({"top": {"a": {"b": {"c": {"d": {"e": {"value": 35}}}}}}})";

        uint8_t    value;
        JsonMapper deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["top"]["a"]["b"]["c"]["d"]["e"]["value"](value, 0));

        EXPECT_EQ(35, value);
    }

    // IsSerializing() / IsDeserializing()
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestSerializeIsSerializing)
    {
        JsonMapper serializer = JsonMapper::Serialize();

        EXPECT_TRUE(serializer.IsSerializing());
        EXPECT_FALSE(serializer.IsDeserializing());
    }

    TEST(JsonMapperTests, TestSerializeIsSerializingChild)
    {
        JsonMapper serializer = JsonMapper::Serialize();

        EXPECT_TRUE(serializer["child"].IsSerializing());
        EXPECT_FALSE(serializer["child"].IsDeserializing());
    }

    TEST(JsonMapperTests, TestSerializeIsSerializingChildDeep)
    {
        JsonMapper serializer = JsonMapper::Serialize();

        EXPECT_TRUE(serializer["child"]["a"]["b"]["c"].IsSerializing());
        EXPECT_FALSE(serializer["child"]["a"]["b"]["c"].IsDeserializing());
    }

    TEST(JsonMapperTests, TestSerializeIsSerializingInvalid)
    {
        JsonMapper serializer = JsonMapper::Serialize();
        int        child_val  = 1;
        serializer["child"](child_val);

        EXPECT_TRUE(serializer["child"]["a"]["b"]["c"].IsSerializing());
        EXPECT_FALSE(serializer["child"]["a"]["b"]["c"].IsDeserializing());
    }

    TEST(JsonMapperTests, TestDeserializeIsSerializing)
    {
        std::string json_str   = R"({})";
        JsonMapper  serializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(serializer.IsSerializing());
        EXPECT_TRUE(serializer.IsDeserializing());
    }

    TEST(JsonMapperTests, TestDeserializeIsSerializingChild)
    {
        std::string json_str = R"({"child": 12})";

        JsonMapper serializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(serializer["child"].IsSerializing());
        EXPECT_TRUE(serializer["child"].IsDeserializing());
    }

    TEST(JsonMapperTests, TestDeserializeIsSerializingChildDeep)
    {
        std::string json_str = R"({"child": {"a": {"b": {"c": 5}}}})";

        JsonMapper serializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(serializer["child"]["a"]["b"]["c"].IsSerializing());
        EXPECT_TRUE(serializer["child"]["a"]["b"]["c"].IsDeserializing());
    }

    TEST(JsonMapperTests, TestDeserializeIsSerializingInvalid)
    {
        std::string json_str   = R"({})";
        JsonMapper  serializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(serializer["child"]["a"]["b"]["c"].IsSerializing());
        EXPECT_TRUE(serializer["child"]["a"]["b"]["c"].IsDeserializing());
    }

    // JSON ownership tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperTests, TestSerializerJsonDifferentMemoryAddr)
    {
        JsonMapper serializer = JsonMapper::Serialize();

        EXPECT_TRUE(serializer.IsValid());
    }

    static JsonMapper CreateJsonMapper()
    {
        return JsonMapper::Serialize();
    }

    TEST(JsonMapperTests, TestSerializerOwnsJsonObject)
    {
        JsonMapper serializer = CreateJsonMapper();

        uint8_t     value = 12;
        std::string name  = "goodbye";
        serializer["hello"](value);
        serializer["name"](name);

        const rapidjson::Value& result = serializer.Json();
        EXPECT_EQ(value, static_cast<uint8_t>(result["hello"].GetUint()));
        EXPECT_EQ(name, std::string(result["name"].GetString()));
    }

    static JsonMapper CreateJsonMapperChild()
    {
        return JsonMapper::Serialize()["child"];
    }

    TEST(JsonMapperTests, TestSerializerChildRetainsJsonObject)
    {
        JsonMapper serializer = CreateJsonMapperChild();

        uint8_t     value = 12;
        std::string name  = "goodbye";
        serializer["hello"](value);
        serializer["name"](name);

        const rapidjson::Value& result = serializer.Json();
        EXPECT_EQ(value, static_cast<uint8_t>(result["child"]["hello"].GetUint()));
        EXPECT_EQ(name, std::string(result["child"]["name"].GetString()));
    }

    TEST(JsonMapperTests, TestSerializerProtectsConcurrentModification)
    {
        std::string json_str = R"({"value": 25})";

        JsonMapper deserializer = JsonMapper::Deserialize(json_str);

        int value;
        EXPECT_TRUE(deserializer["value"](value));
        EXPECT_EQ(25, value);
    }

    TEST(JsonMapperTests, TestSerializerProtectsConcurrentModificationWrite)
    {
        JsonMapper serializer = JsonMapper::Serialize();

        int value = 25;
        EXPECT_TRUE(serializer["value"](value));

        EXPECT_EQ(25, static_cast<uint8_t>(serializer.Json()["value"].GetUint()));
    }

};  // namespace devtrace
