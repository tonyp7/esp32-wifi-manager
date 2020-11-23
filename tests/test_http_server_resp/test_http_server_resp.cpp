/**
 * @file test_http_server_resp.cpp
 * @author TheSomeMan
 * @date 2020-11-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_resp.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerResp : public ::testing::Test
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
    TestHttpServerResp();

    ~TestHttpServerResp() override;
};

TestHttpServerResp::TestHttpServerResp()
    : Test()
{
}

TestHttpServerResp::~TestHttpServerResp()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestHttpServerResp, resp_400) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_400();
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_404) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_404();
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_503) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_503();
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_html) // NOLINT
{
    const char *html_content = "qwe";

    const http_server_resp_t resp = http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_TEXT_HTML,
        nullptr,
        strlen(html_content),
        HTTP_CONENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t *>(html_content));
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(3, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(html_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_js_gzipped_with_param) // NOLINT
{
    const char *js_content = "qwe";
    const char *param_str  = "param1=val1";

    const http_server_resp_t resp = http_server_resp_data_in_flash(
        HTTP_CONENT_TYPE_TEXT_JAVASCRIPT,
        param_str,
        strlen(js_content),
        HTTP_CONENT_ENCODING_GZIP,
        reinterpret_cast<const uint8_t *>(js_content));
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_FALSE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(param_str, resp.p_content_type_param);
    ASSERT_EQ(3, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(js_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_with_caching) // NOLINT
{
    const char *p_content     = "qwer";
    const bool  flag_no_cache = false;

    const http_server_resp_t resp = http_server_resp_data_in_static_mem(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t *>(p_content),
        flag_no_cache);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_without_caching) // NOLINT
{
    const char *p_content     = "qwer";
    const bool  flag_no_cache = true;

    const http_server_resp_t resp = http_server_resp_data_in_static_mem(
        HTTP_CONENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t *>(p_content),
        flag_no_cache);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_with_caching) // NOLINT
{
    const char *p_content     = "qwer";
    const bool  flag_no_cache = false;

    const http_server_resp_t resp = http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t *>(p_content),
        flag_no_cache);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_without_caching) // NOLINT
{
    const char *p_content     = "qwer";
    const bool  flag_no_cache = true;

    const http_server_resp_t resp = http_server_resp_data_in_heap(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t *>(p_content),
        flag_no_cache);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t *>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_from_file_css_gzipped) // NOLINT
{
    const char *   p_content = "qwer";
    const socket_t sock      = 5;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_CONENT_TYPE_TEXT_CSS,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_GZIP,
        sock);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONENT_TYPE_TEXT_CSS, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_png) // NOLINT
{
    const char *   p_content = "qwer";
    const socket_t sock      = 6;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_CONENT_TYPE_IMAGE_PNG,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        sock);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONENT_TYPE_IMAGE_PNG, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_svg) // NOLINT
{
    const char *   p_content = "qwere";
    const socket_t sock      = 7;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_CONENT_TYPE_IMAGE_SVG_XML,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        sock);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONENT_TYPE_IMAGE_SVG_XML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(5, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_octet_stream) // NOLINT
{
    const char *   p_content = "qwere";
    const socket_t sock      = 7;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM,
        nullptr,
        strlen(p_content),
        HTTP_CONENT_ENCODING_NONE,
        sock);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(5, resp.content_len);
    ASSERT_EQ(HTTP_CONENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}
