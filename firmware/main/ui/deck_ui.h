/*
 * Touch Deck UI: status bar, page of large buttons, toast line.
 *
 * This module takes the BSP display lock itself, callers do not need to.
 * Callbacks coming from the USB task never touch LVGL directly; they only
 * update state that a LVGL timer renders.
 */
#pragma once

#include "cfg/config.h"
#include "esp_err.h"
#include "ui/imu_gestures.h"
#include "usb/raw_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Builds the deck.
 * config may be NULL, in which case a small built-in deck is shown together
 * with load_error, so a broken config.json never leaves a blank screen.
 */
esp_err_t td_ui_start(const td_config_t *config, const char *load_error);

/* Safe to call from any task: stores state for the next UI refresh. */
void td_ui_on_telemetry(const td_telemetry_t *telemetry);
void td_ui_on_ack(uint8_t seq, uint8_t status, const char *detail);

/* Active window title reported by the agent; switches page when a page's
 * "match" list contains a substring of it. */
void td_ui_on_profile(const char *profile);

/* Tilt gestures from the IMU task. */
void td_ui_on_gesture(td_gesture_t gesture);

/*
 * Re-reads config_path and rebuilds the deck. The load happens under the
 * display lock, so the render timer never sees a half parsed config.
 */
esp_err_t td_ui_reload_from_file(const char *config_path);

#ifdef __cplusplus
}
#endif
