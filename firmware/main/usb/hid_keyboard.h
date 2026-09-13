/*
 * Keyboard/consumer output path.
 *
 * The UI never talks to TinyUSB directly. It queues jobs, a dedicated task
 * drains the queue and spaces the reports out in time. Without that spacing
 * applications drop keys out of fast macros.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TD_MAX_CHORD_KEYS 6  /* HID boot keyboard report holds six keycodes */
#define TD_MAX_TEXT_LEN   96

typedef enum {
    TD_JOB_CHORD = 0,   /* modifiers + up to six keys pressed together */
    TD_JOB_CONSUMER,    /* single consumer control usage */
    TD_JOB_TEXT,        /* type an ASCII string */
    TD_JOB_DELAY,       /* pause inside a macro */
} td_job_type_t;

typedef struct {
    td_job_type_t type;
    union {
        struct {
            uint8_t modifiers;
            uint8_t keys[TD_MAX_CHORD_KEYS];
            uint8_t key_count;
        } chord;
        struct {
            uint16_t usage;
        } consumer;
        struct {
            char text[TD_MAX_TEXT_LEN];
        } text;
        struct {
            uint32_t ms;
        } delay;
    };
} td_hid_job_t;

/* Starts the queue and the sender task. Call after tinyusb_driver_install(). */
esp_err_t td_hid_start(void);

/* Queues one job. Returns ESP_ERR_INVALID_STATE when the host is not connected. */
esp_err_t td_hid_submit(const td_hid_job_t *job);

/* Convenience wrappers used by the UI layer. */
esp_err_t td_hid_send_chord(uint8_t modifiers, const uint8_t *keys, uint8_t key_count);
esp_err_t td_hid_send_consumer(uint16_t usage);
esp_err_t td_hid_send_text(const char *text);

/* True once the USB host has configured the device. */
bool td_hid_is_connected(void);

#ifdef __cplusplus
}
#endif
