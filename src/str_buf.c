/**
 * @file str_buf.c
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "str_buf.h"
#include <stdio.h>

str_buf_t
str_buf_init(char *p_buf, const str_buf_size_t buf_size)
{
    const str_buf_t str_buf = STR_BUF_INIT(p_buf, buf_size);
    return str_buf;
}

str_buf_size_t
str_buf_get_len(const str_buf_t *p_str_buf)
{
    return p_str_buf->idx;
}

bool
str_buf_is_overflow(const str_buf_t *p_str_buf)
{
    if (0 == p_str_buf->size)
    {
        return false;
    }
    if (p_str_buf->idx >= p_str_buf->size)
    {
        return true;
    }
    return false;
}

bool
str_buf_vprintf(str_buf_t *p_str_buf, const char *fmt, va_list args)
{
    if (NULL == p_str_buf)
    {
        return false;
    }
    if ((NULL == p_str_buf->buf) && (0 != p_str_buf->size))
    {
        return false;
    }
    if ((0 == p_str_buf->size) && (NULL != p_str_buf->buf))
    {
        return false;
    }
    if (0 != p_str_buf->size)
    {
        if (p_str_buf->idx >= p_str_buf->size)
        {
            return false;
        }
    }
    if (NULL == fmt)
    {
        return false;
    }
    char *       p_buf   = (NULL != p_str_buf->buf) ? &p_str_buf->buf[p_str_buf->idx] : NULL;
    const size_t max_len = (0 != p_str_buf->size) ? (p_str_buf->size - p_str_buf->idx) : 0;

    const int len = vsnprintf(p_buf, max_len, fmt, args);
    if (len < 0)
    {
        p_str_buf->idx = p_str_buf->size;
        return false;
    }
    p_str_buf->idx += len;
    if (0 != p_str_buf->size)
    {
        if (p_str_buf->idx >= p_str_buf->size)
        {
            p_str_buf->idx = p_str_buf->size;
            return false;
        }
    }
    return true;
}

__attribute__((format(printf, 2, 3))) //
bool
str_buf_printf(str_buf_t *p_str_buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const bool res = str_buf_vprintf(p_str_buf, fmt, args);
    va_end(args);
    return res;
}
