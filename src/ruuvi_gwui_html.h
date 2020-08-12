#ifndef RUUVI_GATEWAY_ESP_RUUVI_GWUI_HTML_H
#define RUUVI_GATEWAY_ESP_RUUVI_GWUI_HTML_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Find embedded file by path.
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
const uint8_t *
embed_files_find(const char *file_path, size_t *pLen, bool *pIsGzipped);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_RUUVI_GWUI_HTML_H
