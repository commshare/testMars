// Copyright (c) 2011, The Toft Authors.
// All rights reserved.
//
// Author: CHEN Feng <chen3feng@gmail.com>

// toft/encoding/hex_test.cpp

#include "utils/toft_encoding_hex.h"

#include "platform/macros.h"

#include "gtest/gtest.h"

namespace bubblefs {
namespace mytoft {

const char kTestData[] = { '\x12', '\x34', '\x56', '\xFF' };

TEST(HexEncoding, Generic)
{
    char result[2 * ARRAY_SIZE(kTestData) + 1] = {};

    char end_char = result[2 * ARRAY_SIZE(kTestData)];
    Hex::Encode(kTestData, kTestData + ARRAY_SIZE(kTestData), result, true);
    EXPECT_EQ(end_char, result[2 * ARRAY_SIZE(kTestData)]);

    result[2 * ARRAY_SIZE(kTestData)] = '\0';
    EXPECT_STREQ("123456FF", result);
}

static bool IsEqual(const std::string& str, const std::vector<char>& v)
{
    return str.size() == v.size() &&
        std::equal(str.begin(), str.end(), v.begin());
}

TEST(HexEncoding, Buffer)
{
    char result[2 * ARRAY_SIZE(kTestData) + 1];
    EXPECT_STREQ("123456FF", Hex::EncodeToBuffer(kTestData, ARRAY_SIZE(kTestData),
                                                 result, true));
}

TEST(HexEncoding, HexEncodeAppend)
{
    std::string result = "0x";
    EXPECT_EQ("0x123456FF", Hex::EncodeAppend(kTestData, ARRAY_SIZE(kTestData),
                                              &result, true));

    std::vector<char> vector_result;
    vector_result.push_back('0');
    vector_result.push_back('x');
    Hex::EncodeAppend(kTestData, ARRAY_SIZE(kTestData), &vector_result, true);
    EXPECT_TRUE(IsEqual(result, vector_result));
}

TEST(HexEncoding, HexEncodeTo)
{
    std::string result = "0x";
    EXPECT_EQ("123456FF", Hex::EncodeTo(kTestData, ARRAY_SIZE(kTestData),
                                        &result, true));

    std::vector<char> vector_result;
    Hex::EncodeTo(kTestData, ARRAY_SIZE(kTestData), &vector_result, true);
    EXPECT_TRUE(IsEqual(result, vector_result));
}

TEST(HexEncoding, Case)
{
    EXPECT_EQ("123456ff", Hex::EncodeAsString(kTestData, ARRAY_SIZE(kTestData), false));
    EXPECT_EQ("123456FF", Hex::EncodeAsString(kTestData, ARRAY_SIZE(kTestData), true));
}

} // namespace mytoft
} // namespace bubblefs