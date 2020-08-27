/**
 * @file esp_log_wrapper.hpp
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_TESTS_ESP_LOG_WRAPPER_HPP
#define RUUVI_TESTS_ESP_LOG_WRAPPER_HPP

#include "esp_log.h"
#include <string>
#include <cstdarg>
#include <cstdio>

using namespace std;

class LogRecord
{
private:
    void
    init_message(const char *fmt, va_list args)
    {
        va_list args2;
        va_copy(args2, args);
        const int len = vsnprintf(nullptr, 0, fmt, args);
        char *    buf { new char[len + 1] {} };
        vsnprintf(buf, len + 1, fmt, args2);
        va_end(args2);
        this->message = string(buf);
        delete[] buf;
    }

public:
    esp_log_level_t level;
    string          tag;
    string          message;

    LogRecord(esp_log_level_t level, const char *tag, const char *fmt, va_list args)
        : level(level)
        , tag(string(tag))
    {
        this->init_message(fmt, args);
    }

    LogRecord(esp_log_level_t level, const char *tag, const char *fmt, ...)
        : level(level)
        , tag(string(tag))
    {
        va_list args;
        va_start(args, fmt);
        this->init_message(fmt, args);
        va_end(args);
    }
};

void
esp_log_wrapper_init();

void
esp_log_wrapper_deinit();

bool
esp_log_wrapper_is_empty();

LogRecord
esp_log_wrapper_pop();

void
esp_log_wrapper_clear();

#endif // RUUVI_TESTS_ESP_LOG_WRAPPER_HPP
