/**
 * @file esp_log_wrapper.cpp
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstdarg>
#include "esp_log.h"
#include "TQueue.hpp"
#include "esp_log_wrapper.hpp"

static TQueue<LogRecord> *gp_esp_log_queue;

void
esp_log_wrapper_init()
{
    gp_esp_log_queue = new TQueue<LogRecord>;
}

void
esp_log_wrapper_deinit()
{
    if (nullptr != gp_esp_log_queue)
    {
        delete gp_esp_log_queue;
        gp_esp_log_queue = nullptr;
    }
}

void
esp_log_wrapper(esp_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogRecord log_record = LogRecord(level, tag, fmt, args);
    va_end(args);
    gp_esp_log_queue->push(log_record);
}

bool
esp_log_wrapper_is_empty()
{
    return gp_esp_log_queue->is_empty();
}

LogRecord
esp_log_wrapper_pop()
{
    return gp_esp_log_queue->pop();
}

void
esp_log_wrapper_clear()
{
    gp_esp_log_queue->clear();
}
