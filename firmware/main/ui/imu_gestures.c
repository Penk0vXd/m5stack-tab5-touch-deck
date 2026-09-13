#include "ui/imu_gestures.h"

#include <math.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "iot_sensor_hub.h"

static const char *TAG = "td_imu";

#define TD_IMU_PERIOD_MS     100
#define TD_TILT_THRESHOLD_G  0.45f  /* tilt past this on the long axis */
#define TD_TILT_RELEASE_G    0.25f  /* must come back inside this to re-arm */
#define TD_LIFT_THRESHOLD_G  0.35f  /* deviation from 1 g counts as movement */
#define TD_COOLDOWN_US       (1200 * 1000)

static td_gesture_cb_t s_callback;
static sensor_handle_t s_sensor;
static bool s_tilt_armed = true;
static int64_t s_last_gesture_us;

static void emit(td_gesture_t gesture)
{
    const int64_t now = esp_timer_get_time();
    if (now - s_last_gesture_us < TD_COOLDOWN_US) {
        return;
    }
    s_last_gesture_us = now;
    if (s_callback != NULL) {
        s_callback(gesture);
    }
}

static void on_sensor_event(void *arg, sensor_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;

    if (event_id != SENSOR_ACCE_DATA_READY || event_data == NULL) {
        return;
    }
    const sensor_data_t *data = (const sensor_data_t *)event_data;
    const float x = data->acce.x;
    const float y = data->acce.y;
    const float z = data->acce.z;

    /* Any noticeable movement counts as "the user is here". */
    const float magnitude = sqrtf(x * x + y * y + z * z);
    if (fabsf(magnitude - 1.0f) > TD_LIFT_THRESHOLD_G) {
        emit(TD_GESTURE_WAKE);
    }

    /* Tilt on the long axis, with hysteresis so one tilt fires once. */
    if (s_tilt_armed) {
        if (x > TD_TILT_THRESHOLD_G) {
            s_tilt_armed = false;
            emit(TD_GESTURE_NEXT_PAGE);
        } else if (x < -TD_TILT_THRESHOLD_G) {
            s_tilt_armed = false;
            emit(TD_GESTURE_PREV_PAGE);
        }
    } else if (fabsf(x) < TD_TILT_RELEASE_G) {
        s_tilt_armed = true;
    }
}

esp_err_t td_imu_start(td_gesture_cb_t callback)
{
    s_callback = callback;

    const bsp_sensor_config_t cfg = {
        .type = IMU_ID,
        .mode = MODE_POLLING,
        .period = TD_IMU_PERIOD_MS,
    };

    esp_err_t err = bsp_sensor_init(&cfg, &s_sensor);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU unavailable: %s", esp_err_to_name(err));
        return err;
    }

    err = iot_sensor_handler_register_with_type(IMU_ID, SENSOR_ACCE_DATA_READY, on_sensor_event,
                                                NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU handler registration failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "tilt gestures active");
    return ESP_OK;
}
