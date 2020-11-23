/*
@file json.c
@brief handles very basic JSON with a minimal footprint on the system

This code is a completely rewritten version of cJSON 1.4.7.
cJSON is licensed under the MIT license:
Copyright (c) 2009 Dave Gamble

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

@see https://github.com/DaveGamble/cJSON
*/

#include "json.h"
#include <stdbool.h>
#include "str_buf.h"

static bool
json_print_char_escaped(str_buf_t *p_str_buf, const char in_chr)
{
    if (in_chr >= '\x20')
    {
        /* normal character, copy */
        return str_buf_printf(p_str_buf, "%c", in_chr);
    }
    else
    {
        return str_buf_printf(p_str_buf, "\\u%04x", in_chr);
    }
}

bool
json_print_escaped_string(str_buf_t *p_str_buf, const char *p_input_str)
{
    if (NULL == p_str_buf)
    {
        return false;
    }
    if ((NULL == p_str_buf->buf) || (0 == p_str_buf->size))
    {
        return false;
    }
    if (NULL == p_input_str)
    {
        return str_buf_printf(p_str_buf, "\"\"");
    }

    str_buf_printf(p_str_buf, "\"");
    for (const char *in_ptr = p_input_str; '\0' != *in_ptr; ++in_ptr)
    {
        const char in_chr = *in_ptr;
        bool       res    = false;
        switch (in_chr)
        {
            case '\\':
            case '\"':
                res = str_buf_printf(p_str_buf, "\\%c", in_chr);
                break;
            case '\b':
                res = str_buf_printf(p_str_buf, "\\b");
                break;
            case '\f':
                res = str_buf_printf(p_str_buf, "\\f");
                break;
            case '\n':
                res = str_buf_printf(p_str_buf, "\\n");
                break;
            case '\r':
                res = str_buf_printf(p_str_buf, "\\r");
                break;
            case '\t':
                res = str_buf_printf(p_str_buf, "\\t");
                break;
            default:
                res = json_print_char_escaped(p_str_buf, in_chr);
                break;
        }
        if (!res)
        {
            return false;
        }
    }
    if (!str_buf_printf(p_str_buf, "\""))
    {
        return false;
    }
    return true;
}
