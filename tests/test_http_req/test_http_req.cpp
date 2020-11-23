/**
 * @file test_http_req.cpp
 * @author TheSomeMan
 * @date 2020-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_req.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestHttpReq : public ::testing::Test
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
    TestHttpReq();

    ~TestHttpReq() override;
};

TestHttpReq::TestHttpReq()
    : Test()
{
}

TestHttpReq::~TestHttpReq()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestHttpReq, test_lf) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\n"
          "Host: www.msftconnecttest.com\n"
          "Connection: keep-alive\n"
          "Accept: text/plain\n"
          "Accept-Encoding: gzip, deflate\n"
          "Accept-Language: en-US\n"
          "Cache-Control: no-cache, no-store, must-revalidate\n"
          "Content-Type: text/plain\n"
          "User-Agent: Mozilla/5.0\n"
          "\n";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(string("GET"), req_info.http_cmd.ptr);
    ASSERT_EQ(string("/connecttest.txt?n=1605859162338"), req_info.http_uri.ptr);
    ASSERT_EQ(string("HTTP/1.1"), req_info.http_ver.ptr);
    ASSERT_EQ(
        string("Host: www.msftconnecttest.com\n"
               "Connection: keep-alive\n"
               "Accept: text/plain\n"
               "Accept-Encoding: gzip, deflate\n"
               "Accept-Language: en-US\n"
               "Cache-Control: no-cache, no-store, must-revalidate\n"
               "Content-Type: text/plain\n"
               "User-Agent: Mozilla/5.0"),
        req_info.http_header.ptr);
    ASSERT_EQ(string(""), req_info.http_body.ptr);
}

TEST_F(TestHttpReq, test_crlf) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\r\n"
          "Host: www.msftconnecttest.com\r\n"
          "Connection: keep-alive\r\n"
          "Accept: text/plain\r\n"
          "Accept-Encoding: gzip, deflate\r\n"
          "Accept-Language: en-US\r\n"
          "Cache-Control: no-cache, no-store, must-revalidate\r\n"
          "Content-Type: text/plain\r\n"
          "User-Agent: Mozilla/5.0\r\n"
          "\r\n";

    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(string("GET"), req_info.http_cmd.ptr);
    ASSERT_EQ(string("/connecttest.txt?n=1605859162338"), req_info.http_uri.ptr);
    ASSERT_EQ(string("HTTP/1.1"), req_info.http_ver.ptr);
    ASSERT_EQ(
        string("Host: www.msftconnecttest.com\r\n"
               "Connection: keep-alive\r\n"
               "Accept: text/plain\r\n"
               "Accept-Encoding: gzip, deflate\r\n"
               "Accept-Language: en-US\r\n"
               "Cache-Control: no-cache, no-store, must-revalidate\r\n"
               "Content-Type: text/plain\r\n"
               "User-Agent: Mozilla/5.0"),
        req_info.http_header.ptr);
    ASSERT_EQ(string(""), req_info.http_body.ptr);
}

TEST_F(TestHttpReq, test_lf_without_header) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\n"
          "\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(string("GET"), req_info.http_cmd.ptr);
    ASSERT_EQ(string("/connecttest.txt?n=1605859162338"), req_info.http_uri.ptr);
    ASSERT_EQ(string("HTTP/1.1"), req_info.http_ver.ptr);
    ASSERT_EQ(string(""), req_info.http_header.ptr);
    ASSERT_EQ(string("body"), req_info.http_body.ptr);
}

TEST_F(TestHttpReq, test_crlf_without_header) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\r\n"
          "\r\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(string("GET"), req_info.http_cmd.ptr);
    ASSERT_EQ(string("/connecttest.txt?n=1605859162338"), req_info.http_uri.ptr);
    ASSERT_EQ(string("HTTP/1.1"), req_info.http_ver.ptr);
    ASSERT_EQ(string(""), req_info.http_header.ptr);
    ASSERT_EQ(string("body"), req_info.http_body.ptr);
}

TEST_F(TestHttpReq, test_lf_without_body) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\n"
          "Host: www.msftconnecttest.com\n";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_crlf_without_body) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\r\n"
          "Host: www.msftconnecttest.com\r\n";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_lf_bad_req_cmd) // NOLINT
{
    char req[]
        = "GET/connecttest.txt?n=1605859162338HTTP/1.1\n"
          "Host: www.msftconnecttest.com\n"
          "Connection: keep-alive\n"
          "Accept: text/plain\n"
          "Accept-Encoding: gzip, deflate\n"
          "\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_crlf_bad_req_cmd) // NOLINT
{
    char req[]
        = "GET/connecttest.txt?n=1605859162338HTTP/1.1\r\n"
          "Host: www.msftconnecttest.com\r\n"
          "Connection: keep-alive\r\n"
          "Accept: text/plain\r\n"
          "Accept-Encoding: gzip, deflate\r\n"
          "\r\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_lf_bad_req_uri) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338HTTP/1.1\n"
          "Host: www.msftconnecttest.com\n"
          "Connection: keep-alive\n"
          "Accept: text/plain\n"
          "Accept-Encoding: gzip, deflate\n"
          "\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_crlf_bad_req_uri) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338HTTP/1.1\r\n"
          "Host: www.msftconnecttest.com\r\n"
          "Connection: keep-alive\r\n"
          "Accept: text/plain\r\n"
          "Accept-Encoding: gzip, deflate\r\n"
          "\r\n"
          "body";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_FALSE(req_info.is_success);
}

TEST_F(TestHttpReq, test_lf_http_req_header_get_field) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\n"
          "Host: www.msftconnecttest.com\n"
          "Connection: keep-alive\n"
          "Accept: text/plain\n"
          "Accept-Encoding: gzip, deflate\n"
          "Accept-Language: en-US\n"
          "Cache-Control: no-cache, no-store, must-revalidate\n"
          "Content-Type: text/plain\n"
          "User-Agent: Mozilla/5.0\n"
          "\n";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(
        string("Host: www.msftconnecttest.com\n"
               "Connection: keep-alive\n"
               "Accept: text/plain\n"
               "Accept-Encoding: gzip, deflate\n"
               "Accept-Language: en-US\n"
               "Cache-Control: no-cache, no-store, must-revalidate\n"
               "Content-Type: text/plain\n"
               "User-Agent: Mozilla/5.0"),
        req_info.http_header.ptr);

    {
        uint32_t    host_len = 0;
        const char *p_host   = http_req_header_get_field(req_info.http_header, "Host:", &host_len);
        ASSERT_NE(nullptr, p_host);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", host_len, p_host);
        string exp_host = string("www.msftconnecttest.com");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), host_len);
    }
    {
        uint32_t    conn_len = 0;
        const char *p_conn   = http_req_header_get_field(req_info.http_header, "Connection:", &conn_len);
        ASSERT_NE(nullptr, p_conn);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", conn_len, p_conn);
        string exp_host = string("keep-alive");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), conn_len);
    }
    {
        uint32_t    user_agent_len = 0;
        const char *p_user_agent   = http_req_header_get_field(req_info.http_header, "User-Agent:", &user_agent_len);
        ASSERT_NE(nullptr, p_user_agent);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", user_agent_len, p_user_agent);
        string exp_host = string("Mozilla/5.0");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), user_agent_len);
    }
    {
        uint32_t    none_field_len = 1;
        const char *p_none_field   = http_req_header_get_field(
            req_info.http_header,
            "Non-existent-field:",
            &none_field_len);
        ASSERT_EQ(nullptr, p_none_field);
        ASSERT_EQ(0, none_field_len);
    }
}

TEST_F(TestHttpReq, test_crlf_http_req_header_get_field) // NOLINT
{
    char req[]
        = "GET /connecttest.txt?n=1605859162338 HTTP/1.1\r\n"
          "Host: www.msftconnecttest.com\r\n"
          "Connection: keep-alive\r\n"
          "Accept: text/plain\r\n"
          "Accept-Encoding: gzip, deflate\r\n"
          "Accept-Language: en-US\r\n"
          "Cache-Control: no-cache, no-store, must-revalidate\r\n"
          "Content-Type: text/plain\r\n"
          "User-Agent: Mozilla/5.0\r\n"
          "\r\n";
    const http_req_info_t req_info = http_req_parse(req);

    ASSERT_TRUE(req_info.is_success);
    ASSERT_EQ(
        string("Host: www.msftconnecttest.com\r\n"
               "Connection: keep-alive\r\n"
               "Accept: text/plain\r\n"
               "Accept-Encoding: gzip, deflate\r\n"
               "Accept-Language: en-US\r\n"
               "Cache-Control: no-cache, no-store, must-revalidate\r\n"
               "Content-Type: text/plain\r\n"
               "User-Agent: Mozilla/5.0"),
        req_info.http_header.ptr);

    {
        uint32_t    host_len = 0;
        const char *p_host   = http_req_header_get_field(req_info.http_header, "Host:", &host_len);
        ASSERT_NE(nullptr, p_host);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", host_len, p_host);
        string exp_host = string("www.msftconnecttest.com");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), host_len);
    }
    {
        uint32_t    conn_len = 0;
        const char *p_conn   = http_req_header_get_field(req_info.http_header, "Connection:", &conn_len);
        ASSERT_NE(nullptr, p_conn);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", conn_len, p_conn);
        string exp_host = string("keep-alive");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), conn_len);
    }
    {
        uint32_t    user_agent_len = 0;
        const char *p_user_agent   = http_req_header_get_field(req_info.http_header, "User-Agent:", &user_agent_len);
        ASSERT_NE(nullptr, p_user_agent);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.*s", user_agent_len, p_user_agent);
        string exp_host = string("Mozilla/5.0");
        ASSERT_EQ(exp_host, string(buf));
        ASSERT_EQ(exp_host.length(), user_agent_len);
    }
    {
        uint32_t    none_field_len = 1;
        const char *p_none_field   = http_req_header_get_field(
            req_info.http_header,
            "Non-existent-field:",
            &none_field_len);
        ASSERT_EQ(nullptr, p_none_field);
        ASSERT_EQ(0, none_field_len);
    }
}
