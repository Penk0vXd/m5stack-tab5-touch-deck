/*
 * Tilt gestures from the BMI270.
 *
 * Two gestures only, because anything richer fires by accident while typing on
 * the deck: tilt left or right past a threshold switches page, and picking the
 * device up wakes the screen.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TD_GESTURE_NONE = 0,
    TD_GESTURE_NEXT_PAGE,
    TD_GESTURE_PREV_PAGE,
    TD_GESTURE_WAKE,
} td_gesture_t;

typedef void (*td_gesture_cb_t)(td_gesture_t gesture);

/* Starts the IMU in polling mode. Returns an error when the sensor is absent;
 * the deck stays fully usable without it. */
esp_err_t td_imu_start(td_gesture_cb_t callback);

#ifdef __cplusplus
}
#endif
