/**
 * @file test_str_buf.cpp
 * @author TheSomeMan
 * @date 2020-08-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "str_buf.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestStrBuf : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

public:
    TestStrBuf();

    ~TestStrBuf() override;
};

TestStrBuf::TestStrBuf()
    : Test()
{
}

TestStrBuf::~TestStrBuf() = default;

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestStrBuf, test_str_buf_init) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf  = STR_BUF_INIT_WITH_ARR(tmp_buf);
    str_buf_t str_buf2 = str_buf_init(tmp_buf, sizeof(tmp_buf));
    ASSERT_EQ(str_buf.buf, str_buf2.buf);
    ASSERT_EQ(str_buf.size, str_buf2.size);
    ASSERT_EQ(str_buf.idx, str_buf2.idx);

    ASSERT_EQ(&tmp_buf[0], str_buf.buf);
    ASSERT_EQ(sizeof(tmp_buf), str_buf.size);
    ASSERT_EQ(0, str_buf.idx);

    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_simple_print) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_TRUE(str_buf_printf(&str_buf, "abc"));
    ASSERT_EQ(string("abc"), string(tmp_buf));
    ASSERT_EQ(strlen("abc"), str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_print_twice) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_TRUE(str_buf_printf(&str_buf, "abc"));
    ASSERT_TRUE(str_buf_printf(&str_buf, "def"));
    ASSERT_EQ(string("abcdef"), string(tmp_buf));
    ASSERT_EQ(strlen("abcdef"), str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_printf) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_TRUE(str_buf_printf(&str_buf, "%s %d", "xyz", 123));
    ASSERT_EQ(string("xyz 123"), string(tmp_buf));
    ASSERT_EQ(strlen("xyz 123"), str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_printf_full_buf) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_TRUE(str_buf_printf(&str_buf, "%s %d", "abcdef", 12));
    ASSERT_EQ(string("abcdef 12"), string(tmp_buf));
    ASSERT_EQ(9, str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));

    ASSERT_TRUE(str_buf_printf(&str_buf, "%s", ""));
    ASSERT_EQ(string("abcdef 12"), string(tmp_buf));
    ASSERT_EQ(9, str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_printf_overflow_1) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_FALSE(str_buf_printf(&str_buf, "%s %d", "abcdef", 123));
    ASSERT_EQ(string("abcdef 12"), string(tmp_buf));
    ASSERT_EQ(10, str_buf_get_len(&str_buf));
    ASSERT_TRUE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_printf_overflow_2) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(tmp_buf);
    ASSERT_FALSE(str_buf_printf(&str_buf, "%s %d", "abcdef", 1234));
    ASSERT_EQ(string("abcdef 12"), string(tmp_buf));
    ASSERT_EQ(10, str_buf_get_len(&str_buf));
    ASSERT_TRUE(str_buf_is_overflow(&str_buf));

    ASSERT_FALSE(str_buf_printf(&str_buf, "Q"));
    ASSERT_EQ(string("abcdef 12"), string(tmp_buf));
    ASSERT_EQ(10, str_buf_get_len(&str_buf));
    ASSERT_TRUE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_calc_length) // NOLINT
{
    str_buf_t str_buf = STR_BUF_INIT(nullptr, 0);
    ASSERT_TRUE(str_buf_printf(&str_buf, "abc"));
    ASSERT_EQ(strlen("abc"), str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));

    ASSERT_TRUE(str_buf_printf(&str_buf, "def"));
    ASSERT_EQ(strlen("abcdef"), str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
}

TEST_F(TestStrBuf, test_null_str_buf) // NOLINT
{
    ASSERT_FALSE(str_buf_printf(nullptr, "abc"));
}

TEST_F(TestStrBuf, test_incorrect_str_buf_null_buf) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT(nullptr, sizeof(tmp_buf));
    ASSERT_FALSE(str_buf_printf(&str_buf, "abc"));
    ASSERT_EQ(0, str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
    ASSERT_EQ(string("undef"), string(tmp_buf));
}

TEST_F(TestStrBuf, test_incorrect_str_buf_zero_size) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT(tmp_buf, 0);
    ASSERT_FALSE(str_buf_printf(&str_buf, "abc"));
    ASSERT_EQ(0, str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
    ASSERT_EQ(string("undef"), string(tmp_buf));
}

TEST_F(TestStrBuf, test_fmt_null) // NOLINT
{
    char tmp_buf[10] = "undef";

    str_buf_t str_buf = STR_BUF_INIT(tmp_buf, sizeof(tmp_buf));
    ASSERT_FALSE(str_buf_printf(&str_buf, nullptr));
    ASSERT_EQ(0, str_buf_get_len(&str_buf));
    ASSERT_FALSE(str_buf_is_overflow(&str_buf));
    ASSERT_EQ(string("undef"), string(tmp_buf));
}

TEST_F(TestStrBuf, test_negative_snprintf) // NOLINT
{
    char tmp_buf[20] = "undef";

    str_buf_t str_buf = STR_BUF_INIT(tmp_buf, sizeof(tmp_buf));
    ASSERT_FALSE(str_buf_printf(&str_buf, "TestABC %ls", L"\U10FFFFFF"));
    ASSERT_EQ(sizeof(tmp_buf), str_buf_get_len(&str_buf));
    ASSERT_TRUE(str_buf_is_overflow(&str_buf));
    ASSERT_EQ(string("TestABC "), string(tmp_buf));
}
