/**
 * @file esp_log.h
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP_LOG_H
#define ESP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

void
esp_log_wrapper(esp_log_level_t level, const char *tag, const char *fmt, ...);

#define ESP_LOGE(tag, fmt, ...) esp_log_wrapper(ESP_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) esp_log_wrapper(ESP_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) esp_log_wrapper(ESP_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) esp_log_wrapper(ESP_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) esp_log_wrapper(ESP_LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // ESP_LOG_H
