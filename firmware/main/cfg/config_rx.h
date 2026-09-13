/*
 * Receives config.json over the raw HID channel.
 *
 * The agent sends BEGIN, a stream of CHUNK packets and END. Bytes are written
 * to a temporary file and only swapped in when the whole upload parses, so a
 * dropped cable never leaves a half written config on the device.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called after a successful file swap. ACK is sent only if the UI reloads it. */
typedef esp_err_t (*td_config_applied_cb_t)(void);

esp_err_t td_config_rx_start(const char *config_path, td_config_applied_cb_t on_applied);

#ifdef __cplusplus
}
#endif
