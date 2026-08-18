// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the JSON mapper array methods.

#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <dev_trace_common.h>
#include <json/json_mapper.h>

namespace devtrace
{
    struct ArrayTestItem
    {
        int         value = 0;
        std::string name  = "";

        ArrayTestItem() = default;
        ArrayTestItem(int v, std::string s)
            : value(v)
            , name(s)
        {
        }
    };

    // Serializers
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static bool SerializeUint8(uint8_t& item, JsonMapper item_serializer)
    {
        return item_serializer(item, 25);
    }

    static bool SerializeArrayItem(ArrayTestItem& item, JsonMapper item_serializer)
    {
        return item_serializer["value"](item.value) && item_serializer["name"](item.name);
    }

    static bool BadSerializeUint8([[maybe_unused]] uint8_t& item, [[maybe_unused]] JsonMapper item_serializer)
    {
        return false;
    }

    // Utils
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static void VerifyArraysEqual(const std::vector<ArrayTestItem>& item, const rapidjson::Value& array)
    {
        ASSERT_TRUE(array.IsArray());
        ASSERT_EQ(item.size(), array.Size());

        for (rapidjson::SizeType i = 0; i < array.Size(); i++)
        {
            EXPECT_EQ(item[i].value, array[i]["value"].GetInt());
            EXPECT_EQ(item[i].name, std::string(array[i]["name"].GetString()));
        }
    }

    // Simple array writes
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestWriteEmptyArrayPrimitive)
    {
        std::vector<uint8_t> empty;
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(empty, SerializeUint8));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());
        EXPECT_TRUE(result["arr"].Empty());
    }

    TEST(JsonMapperArrayTests, TestWriteOneElementPrimitive)
    {
        std::vector<uint8_t> array      = {123};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, SerializeUint8));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());

        const rapidjson::Value& result_arr = result["arr"];
        ASSERT_EQ(array.size(), result_arr.Size());
        EXPECT_EQ(array[0], static_cast<uint8_t>(result_arr[0u].GetUint()));
    }

    TEST(JsonMapperArrayTests, TestWriteMultipleElementsPrimitive)
    {
        std::vector<uint8_t> array      = {123, 8, 21, 74};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, SerializeUint8));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());

        const rapidjson::Value& result_arr = result["arr"];
        ASSERT_EQ(array.size(), result_arr.Size());
        for (rapidjson::SizeType i = 0; i < result_arr.Size(); i++)
        {
            EXPECT_EQ(array[i], static_cast<uint8_t>(result_arr[i].GetUint()));
        }
    }

    TEST(JsonMapperArrayTests, TestWriteEmptyArrayStruct)
    {
        std::vector<ArrayTestItem> empty;
        JsonMapper                 serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<ArrayTestItem>(empty, SerializeArrayItem));
        VerifyArraysEqual(empty, serializer.Json()["arr"]);
    }

    TEST(JsonMapperArrayTests, TestWriteOneElementStruct)
    {
        std::vector<ArrayTestItem> array      = {{123, "cool"}};
        JsonMapper                 serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<ArrayTestItem>(array, SerializeArrayItem));
        VerifyArraysEqual(array, serializer.Json()["arr"]);
    }

    TEST(JsonMapperArrayTests, TestWriteMultipleElementsStruct)
    {
        std::vector<ArrayTestItem> array      = {{123, "cool"}, {0, "bad"}, {8, "color"}};
        JsonMapper                 serializer = JsonMapper::Serialize();
        serializer["arr"].Arr<ArrayTestItem>(array, SerializeArrayItem);
        VerifyArraysEqual(array, serializer.Json()["arr"]);
    }

    // Simple array writes with defaults
    // The default argument doesn't do anything for writes, but these tests exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestWriteEmptyArrayPrimitiveWithDefault)
    {
        std::vector<uint8_t> empty;
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(empty, SerializeUint8, {}));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());
        EXPECT_TRUE(result["arr"].Empty());
    }

    TEST(JsonMapperArrayTests, TestWriteOneElementPrimitiveWithDefault)
    {
        std::vector<uint8_t> array      = {123};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, SerializeUint8, {}));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());

        const rapidjson::Value& result_arr = result["arr"];
        ASSERT_EQ(array.size(), result_arr.Size());
        EXPECT_EQ(array[0], static_cast<uint8_t>(result_arr[0u].GetUint()));
    }

    TEST(JsonMapperArrayTests, TestWriteMultipleElementsPrimitiveWithDefault)
    {
        std::vector<uint8_t> array      = {123, 8, 21, 74};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, SerializeUint8, {}));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());

        const rapidjson::Value& result_arr = result["arr"];
        ASSERT_EQ(array.size(), result_arr.Size());
        for (rapidjson::SizeType i = 0; i < result_arr.Size(); i++)
        {
            EXPECT_EQ(array[i], static_cast<uint8_t>(result_arr[i].GetUint()));
        }
    }

    TEST(JsonMapperArrayTests, TestWriteEmptyArrayStructWithDefault)
    {
        std::vector<ArrayTestItem> empty;
        JsonMapper                 serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<ArrayTestItem>(empty, SerializeArrayItem, {}));
        VerifyArraysEqual(empty, serializer.Json()["arr"]);
    }

    TEST(JsonMapperArrayTests, TestWriteOneElementStructWithDefault)
    {
        std::vector<ArrayTestItem> array      = {{123, "cool"}};
        JsonMapper                 serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<ArrayTestItem>(array, SerializeArrayItem, {}));
        VerifyArraysEqual(array, serializer.Json()["arr"]);
    }

    TEST(JsonMapperArrayTests, TestWriteMultipleElementsStructWithDefault)
    {
        std::vector<ArrayTestItem> array      = {{123, "cool"}, {0, "bad"}, {8, "color"}};
        JsonMapper                 serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<ArrayTestItem>(array, SerializeArrayItem, {}));
        VerifyArraysEqual(array, serializer.Json()["arr"]);
    }

    // Array reads
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestReadEmptyPrimitive)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadSingleItemPrimitive)
    {
        std::vector<uint8_t> array    = {45};
        std::string          json_str = R"({"arr": [45]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8));

        ASSERT_EQ(array.size(), result.size());
        for (int i = 0; i < static_cast<int>(array.size()); i++)
        {
            EXPECT_EQ(array[i], result[i]);
        }
    }

    TEST(JsonMapperArrayTests, TestReadMultipleItemPrimitive)
    {
        std::vector<uint8_t> array    = {45, 128, 0, 14, 234};
        std::string          json_str = R"({"arr": [45, 128, 0, 14, 234]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8));

        ASSERT_EQ(array.size(), result.size());
        for (int i = 0; i < static_cast<int>(array.size()); i++)
        {
            EXPECT_EQ(array[i], result[i]);
        }
    }

    TEST(JsonMapperArrayTests, TestReadEmptyStruct)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadSingleItemStruct)
    {
        std::string json_str = R"({"arr": [{"value": 21, "name": "cool"}]})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem));

        ASSERT_EQ(1, result.size());

        EXPECT_EQ(21, result[0].value);
        EXPECT_EQ("cool", result[0].name);
    }

    TEST(JsonMapperArrayTests, TestReadMultipleItemsStruct)
    {
        std::string json_str =
            R"({"arr": [{"value": 21, "name": "cool"}, {"value": 128, "name": "sun"}, {"value": 63, "name": "moon"}, {"value": 0, "name": "empty"}]})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem));

        ASSERT_EQ(4, result.size());

        EXPECT_EQ(21, result[0].value);
        EXPECT_EQ("cool", result[0].name);

        EXPECT_EQ(128, result[1].value);
        EXPECT_EQ("sun", result[1].name);

        EXPECT_EQ(63, result[2].value);
        EXPECT_EQ("moon", result[2].name);

        EXPECT_EQ(0, result[3].value);
        EXPECT_EQ("empty", result[3].name);
    }

    // Array reads with defaults
    // None of these tests actually leverage the default, they exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestReadEmptyPrimitiveWithDefault)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8, {}));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadSingleItemPrimitiveWithDefault)
    {
        std::vector<uint8_t> array    = {45};
        std::string          json_str = R"({"arr": [45]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8, {}));

        ASSERT_EQ(array.size(), result.size());
        for (int i = 0; i < static_cast<int>(array.size()); i++)
        {
            EXPECT_EQ(array[i], result[i]);
        }
    }

    TEST(JsonMapperArrayTests, TestReadMultipleItemPrimitiveWithDefault)
    {
        std::vector<uint8_t> array    = {45, 128, 0, 14, 234};
        std::string          json_str = R"({"arr": [45, 128, 0, 14, 234]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, SerializeUint8, {}));

        ASSERT_EQ(array.size(), result.size());
        for (int i = 0; i < static_cast<int>(array.size()); i++)
        {
            EXPECT_EQ(array[i], result[i]);
        }
    }

    TEST(JsonMapperArrayTests, TestReadEmptyStructWithDefault)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem, {}));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadSingleItemStructWithDefault)
    {
        std::string json_str = R"({"arr": [{"value": 21, "name": "cool"}]})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem, {}));

        ASSERT_EQ(1, result.size());

        EXPECT_EQ(21, result[0].value);
        EXPECT_EQ("cool", result[0].name);
    }

    TEST(JsonMapperArrayTests, TestReadMultipleItemsStructWithDefault)
    {
        std::string json_str =
            R"({"arr": [{"value": 21, "name": "cool"}, {"value": 128, "name": "sun"}, {"value": 63, "name": "moon"}, {"value": 0, "name": "empty"}]})";

        std::vector<ArrayTestItem> result;
        JsonMapper                 deserializer = JsonMapper::Deserialize(json_str);
        EXPECT_TRUE(deserializer["arr"].Arr<ArrayTestItem>(result, SerializeArrayItem, {}));

        ASSERT_EQ(4, result.size());

        EXPECT_EQ(21, result[0].value);
        EXPECT_EQ("cool", result[0].name);

        EXPECT_EQ(128, result[1].value);
        EXPECT_EQ("sun", result[1].name);

        EXPECT_EQ(63, result[2].value);
        EXPECT_EQ("moon", result[2].name);

        EXPECT_EQ(0, result[3].value);
        EXPECT_EQ("empty", result[3].name);
    }

    // Special array writes
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestWriteSucceedsWithNoItemsBadSerializer)
    {
        std::vector<uint8_t> array      = {};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());
        EXPECT_TRUE(result["arr"].Empty());
    }

    TEST(JsonMapperArrayTests, TestWriteFailsWhenOneItemFails)
    {
        std::vector<uint8_t> array      = {123};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_FALSE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8));
    }

    TEST(JsonMapperArrayTests, TestWriteFailsWhenMultipleItemsFails)
    {
        std::vector<uint8_t> array      = {123, 0, 12};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_FALSE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8));
    }

    TEST(JsonMapperArrayTests, TestWriteFailMultipleItemsOneFails)
    {
        std::vector<uint8_t> array      = {123, 0, 12};
        JsonMapper           serializer = JsonMapper::Serialize();
        serializer["arr"].Arr<uint8_t>(array, [](uint8_t& value, JsonMapper serializer) {
            if (value == 0)
            {
                return false;
            }

            return serializer(value);
        });
    }

    // Special array writes
    // The default argument doesn't do anything for writes, but these tests exist for coverage
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestWriteSucceedsWithNoItemsBadSerializerWithDefault)
    {
        std::vector<uint8_t> array      = {};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_TRUE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8, {}));

        const rapidjson::Value& result = serializer.Json();
        EXPECT_TRUE(result["arr"].IsArray());
        EXPECT_TRUE(result["arr"].Empty());
    }

    TEST(JsonMapperArrayTests, TestWriteFailsWhenOneItemFailsWithDefault)
    {
        std::vector<uint8_t> array      = {123};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_FALSE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8, {}));
    }

    TEST(JsonMapperArrayTests, TestWriteFailsWhenMultipleItemsFailsWithDefault)
    {
        std::vector<uint8_t> array      = {123, 0, 12};
        JsonMapper           serializer = JsonMapper::Serialize();
        EXPECT_FALSE(serializer["arr"].Arr<uint8_t>(array, BadSerializeUint8, {}));
    }

    TEST(JsonMapperArrayTests, TestWriteFailMultipleItemsOneFailsWithDefault)
    {
        std::vector<uint8_t> array      = {123, 0, 12};
        JsonMapper           serializer = JsonMapper::Serialize();
        serializer["arr"].Arr<uint8_t>(array,
                                       [](uint8_t& value, JsonMapper serializer) {
                                           if (value == 0)
                                           {
                                               return false;
                                           }

                                           return serializer(value);
                                       },
                                       {});
    }

    // Special array reads
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestReadPassesEmptyWithBadSerialize)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenOneItemFails)
    {
        std::vector<uint8_t> array    = {123};
        std::string          json_str = R"({"arr": [123]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenMultipleItemsFails)
    {
        std::vector<uint8_t> array    = {123, 0, 53, 200};
        std::string          json_str = R"({"arr": [123, 0, 53, 200]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenOnlyOneItemFails)
    {
        std::vector<uint8_t> array    = {123, 0, 53, 200};
        std::string          json_str = R"({"arr": [123, 0, 53, 200]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(deserializer["arr"].Arr<uint8_t>(result, [](uint8_t& item, JsonMapper item_deserializer) {
            item_deserializer(item);
            if (item == 0)
            {
                return false;
            }

            return true;
        }));

        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenNotArray)
    {
        std::string json_str = R"({"arr": "hello"})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadClearsInputIfFail)
    {
        std::string json_str = R"({"arr": "hello"})";

        std::vector<uint8_t> result       = {123, 123};
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_FALSE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8));
        EXPECT_TRUE(result.empty());
    }

    // Special array reads with defaults
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST(JsonMapperArrayTests, TestReadPassesEmptyWithBadSerializeWithDefault)
    {
        std::string json_str = R"({"arr": []})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8, {99}));
        EXPECT_TRUE(result.empty());
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenOneItemFailsWithDefault)
    {
        std::vector<uint8_t> array    = {123};
        std::string          json_str = R"({"arr": [123]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8, {99}));

        ASSERT_EQ(1, result.size());
        EXPECT_EQ(99, result[0]);
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenMultipleItemsFailsWithDefault)
    {
        std::vector<uint8_t> array    = {123, 0, 53, 200};
        std::string          json_str = R"({"arr": [123, 0, 53, 200]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8, {99}));

        ASSERT_EQ(1, result.size());
        EXPECT_EQ(99, result[0]);
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenOnlyOneItemFailsWithDefault)
    {
        std::vector<uint8_t> array    = {123, 0, 53, 200};
        std::string          json_str = R"({"arr": [123, 0, 53, 200]})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result,
                                                     [](uint8_t& item, JsonMapper item_deserializer) {
                                                         item_deserializer(item);
                                                         if (item == 0)
                                                         {
                                                             return false;
                                                         }

                                                         return true;
                                                     },
                                                     {99}));

        ASSERT_EQ(1, result.size());
        EXPECT_EQ(99, result[0]);
    }

    TEST(JsonMapperArrayTests, TestReadFailsWhenNotArrayWithDefault)
    {
        std::string json_str = R"({"arr": "hello"})";

        std::vector<uint8_t> result;
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8, {99}));

        ASSERT_EQ(1, result.size());
        EXPECT_EQ(99, result[0]);
    }

    TEST(JsonMapperArrayTests, TestReadClearsInputIfFailWithDefault)
    {
        std::string json_str = R"({"arr": "hello"})";

        std::vector<uint8_t> result       = {123, 123};
        JsonMapper           deserializer = JsonMapper::Deserialize(json_str);

        EXPECT_TRUE(deserializer["arr"].Arr<uint8_t>(result, BadSerializeUint8, {99}));

        ASSERT_EQ(1, result.size());
        EXPECT_EQ(99, result[0]);
    }

};  // namespace devtrace
