/**
 * @file str_buf.h
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef STR_BUF_H
#define STR_BUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STR_BUF_INIT(buf_, len_) \
    { \
        .buf = (buf_), .size = (len_), .idx = 0, \
    }

#define STR_BUF_INIT_WITH_ARR(arr_) STR_BUF_INIT((arr_), sizeof(arr_))

typedef size_t str_buf_size_t;

typedef struct str_buf_t
{
    char *         buf;
    str_buf_size_t size;
    str_buf_size_t idx;
} str_buf_t;

str_buf_t
str_buf_init(char *p_buf, const str_buf_size_t buf_size);

str_buf_size_t
str_buf_get_len(const str_buf_t *p_str_buf);

bool
str_buf_is_overflow(const str_buf_t *p_str_buf);

bool
str_buf_vprintf(str_buf_t *p_str_buf, const char *fmt, va_list args);

__attribute__((format(printf, 2, 3))) //
bool
str_buf_printf(str_buf_t *p_str_buf, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // STR_BUF_H
