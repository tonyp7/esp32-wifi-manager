/**
 * @file test_json.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestJson : public ::testing::Test
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
    TestJson();

    ~TestJson() override;
};

TestJson::TestJson()
    : Test()
{
}

TestJson::~TestJson()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJson, test_null_output_buf_null_input_buf) // NOLINT
{
    ASSERT_FALSE(json_print_escaped_string(nullptr, nullptr));
}

TEST_F(TestJson, test_null_output_buf_nonnull_input_buf) // NOLINT
{
    ASSERT_FALSE(json_print_escaped_string(nullptr, "abc"));
}

TEST_F(TestJson, test_nullptr_in_output_buf) // NOLINT
{
    str_buf_t str_buf = STR_BUF_INIT(nullptr, 0);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, nullptr));
}

TEST_F(TestJson, test_null_input_buf) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, nullptr));
    ASSERT_EQ(string("\"\""), string(buf));
}

TEST_F(TestJson, test_null_input_buf_and_output_buf_len_0) // NOLINT
{
    char      buf[1];
    str_buf_t str_buf = STR_BUF_INIT(buf, 0);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, nullptr));
}

TEST_F(TestJson, test_null_input_buf_and_output_buf_len_1) // NOLINT
{
    char      buf[1];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, nullptr));
}

TEST_F(TestJson, test_null_input_buf_and_output_buf_len_2) // NOLINT
{
    char      buf[2];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, nullptr));
}

TEST_F(TestJson, test_null_input_buf_and_output_buf_len_3) // NOLINT
{
    char      buf[3];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, nullptr));
    ASSERT_EQ(string("\"\""), string(buf));
}

TEST_F(TestJson, test_abc) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "abc"));
    ASSERT_EQ(string("\"abc\""), string(buf));
}

TEST_F(TestJson, test_escape_backslash) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\\c"));
    ASSERT_EQ(string("\"a\\\\c\""), string(buf));
}

TEST_F(TestJson, test_escape_quotes) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\"c"));
    ASSERT_EQ(string("\"a\\\"c\""), string(buf));
}

TEST_F(TestJson, test_escape_0x01) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x01Y"));
    ASSERT_EQ(string("\"X\\u0001Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x02) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x02Y"));
    ASSERT_EQ(string("\"X\\u0002Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x03) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x03Y"));
    ASSERT_EQ(string("\"X\\u0003Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x04) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x04Y"));
    ASSERT_EQ(string("\"X\\u0004Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x05) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x05Y"));
    ASSERT_EQ(string("\"X\\u0005Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x06) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x06Y"));
    ASSERT_EQ(string("\"X\\u0006Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x07) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x07Y"));
    ASSERT_EQ(string("\"X\\u0007Y\""), string(buf));
}

TEST_F(TestJson, test_escape_backspace) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\bc"));
    ASSERT_EQ(string("\"a\\bc\""), string(buf));
}

TEST_F(TestJson, test_escape_tab) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\tc"));
    ASSERT_EQ(string("\"a\\tc\""), string(buf));
}

TEST_F(TestJson, test_escape_line_feed) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\nc"));
    ASSERT_EQ(string("\"a\\nc\""), string(buf));
}

TEST_F(TestJson, test_escape_0x0b) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x0bY"));
    ASSERT_EQ(string("\"X\\u000bY\""), string(buf));
}

TEST_F(TestJson, test_escape_form_feed) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\fc"));
    ASSERT_EQ(string("\"a\\fc\""), string(buf));
}

TEST_F(TestJson, test_escape_carriage_return) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "a\rc"));
    ASSERT_EQ(string("\"a\\rc\""), string(buf));
}

TEST_F(TestJson, test_escape_0x0e) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x0eY"));
    ASSERT_EQ(string("\"X\\u000eY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x0f) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x0fY"));
    ASSERT_EQ(string("\"X\\u000fY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x10) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x10Y"));
    ASSERT_EQ(string("\"X\\u0010Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x11) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x11Y"));
    ASSERT_EQ(string("\"X\\u0011Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x12) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x12Y"));
    ASSERT_EQ(string("\"X\\u0012Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x13) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x13Y"));
    ASSERT_EQ(string("\"X\\u0013Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x14) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x14Y"));
    ASSERT_EQ(string("\"X\\u0014Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x15) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x15Y"));
    ASSERT_EQ(string("\"X\\u0015Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x16) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x16Y"));
    ASSERT_EQ(string("\"X\\u0016Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x17) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x17Y"));
    ASSERT_EQ(string("\"X\\u0017Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x18) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x18Y"));
    ASSERT_EQ(string("\"X\\u0018Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x19) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x19Y"));
    ASSERT_EQ(string("\"X\\u0019Y\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1a) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1aY"));
    ASSERT_EQ(string("\"X\\u001aY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1b) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1bY"));
    ASSERT_EQ(string("\"X\\u001bY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1c) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1cY"));
    ASSERT_EQ(string("\"X\\u001cY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1d) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1dY"));
    ASSERT_EQ(string("\"X\\u001dY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1e) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1eY"));
    ASSERT_EQ(string("\"X\\u001eY\""), string(buf));
}

TEST_F(TestJson, test_escape_0x1f) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\x1fY"));
    ASSERT_EQ(string("\"X\\u001fY\""), string(buf));
}

TEST_F(TestJson, test_escape_all) // NOLINT
{
    char      buf[100];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "X\\ \" \b \f \n \r \tY"));
    ASSERT_EQ(string("\"X\\\\ \\\" \\b \\f \\n \\r \\tY\""), string(buf));
}

TEST_F(TestJson, test_ascii) // NOLINT
{
    char   buf[200];
    string ascii_str = {};
    for (int ch = '\x20'; ch <= '\x7F'; ++ch)
    {
        if (ch == '\\')
        {
            continue;
        }
        if (ch == '\"')
        {
            continue;
        }
        ascii_str += char(ch);
    }
    string    quoted_ascii_str = {};
    str_buf_t str_buf          = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, ascii_str.c_str()));
    quoted_ascii_str += "\"";
    quoted_ascii_str += ascii_str;
    quoted_ascii_str += "\"";
    ASSERT_EQ(quoted_ascii_str, string(buf));
}

TEST_F(TestJson, test_overflow_on_closing_quote) // NOLINT
{
    char buf[5];

    memset(buf, 0, sizeof(buf));
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "ab"));
    ASSERT_EQ("\"ab\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "abc"));
}

TEST_F(TestJson, test_overflow_on_escaped_char) // NOLINT
{
    char      buf[9];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\x1f"));
    ASSERT_EQ("\"\\u001f\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\x1f"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\x1f"));
}

TEST_F(TestJson, test_overflow_on_unescaped_char) // NOLINT
{
    char      buf[4];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "A"));
    ASSERT_EQ("\"A\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "aA"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "abA"));
}

TEST_F(TestJson, test_overflow_on_escaped_quote) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\""));
    ASSERT_EQ("\"\\\"\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\""));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\""));
}

TEST_F(TestJson, test_overflow_on_escaped_slash) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\\"));
    ASSERT_EQ("\"\\\\\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\\"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\\"));
}

TEST_F(TestJson, test_overflow_on_escaped_ctrl_b) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\b"));
    ASSERT_EQ("\"\\b\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\b"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\b"));
}

TEST_F(TestJson, test_overflow_on_escaped_ctrl_f) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\f"));
    ASSERT_EQ("\"\\f\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\f"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\f"));
}

TEST_F(TestJson, test_overflow_on_escaped_ctrl_n) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\n"));
    ASSERT_EQ("\"\\n\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\n"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\n"));
}

TEST_F(TestJson, test_overflow_on_escaped_ctrl_r) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\r"));
    ASSERT_EQ("\"\\r\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\r"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\r"));
}

TEST_F(TestJson, test_overflow_on_escaped_ctrl_t) // NOLINT
{
    char      buf[5];
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_TRUE(json_print_escaped_string(&str_buf, "\t"));
    ASSERT_EQ("\"\\t\"", string(buf));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "a\t"));

    memset(buf, 0, sizeof(buf));
    str_buf = STR_BUF_INIT_WITH_ARR(buf);
    ASSERT_FALSE(json_print_escaped_string(&str_buf, "ab\t"));
}
