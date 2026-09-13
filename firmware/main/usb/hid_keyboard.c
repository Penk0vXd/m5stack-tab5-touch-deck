#include "usb/hid_keyboard.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "tusb.h"
#include "class/hid/hid_device.h"

#include "usb/usb_descriptors.h"

static const char *TAG = "td_hid";

/*
 * Timing. Press and release of one chord are two reports; applications need a
 * gap between them and between consecutive chords, otherwise keys get dropped.
 */
#define TD_PRESS_HOLD_MS 12
#define TD_INTER_KEY_MS  15
#define TD_QUEUE_DEPTH   16
#define TD_READY_WAIT_MS 200

static QueueHandle_t s_queue;

static const uint8_t k_ascii_to_keycode[128][2] = {HID_ASCII_TO_KEYCODE};

bool td_hid_is_connected(void)
{
    return tud_mounted();
}

/* Waits for the keyboard interface to accept a new report. */
static bool wait_ready(void)
{
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(TD_READY_WAIT_MS);
    while (!tud_hid_n_ready(TD_HID_ITF_KEYBOARD)) {
        if (xTaskGetTickCount() > deadline) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

static void send_keyboard_report(uint8_t modifiers, const uint8_t keys[TD_MAX_CHORD_KEYS])
{
    if (!wait_ready()) {
        ESP_LOGW(TAG, "keyboard interface not ready, report dropped");
        return;
    }
    uint8_t keycodes[6] = {0};
    if (keys != NULL) {
        memcpy(keycodes, keys, TD_MAX_CHORD_KEYS);
    }
    tud_hid_n_keyboard_report(TD_HID_ITF_KEYBOARD, TD_REPORT_ID_KEYBOARD, modifiers, keycodes);
}

static void release_all_keys(void)
{
    send_keyboard_report(0, NULL);
}

static void run_chord(uint8_t modifiers, const uint8_t *keys, uint8_t key_count)
{
    uint8_t keycodes[TD_MAX_CHORD_KEYS] = {0};
    for (uint8_t i = 0; i < key_count && i < TD_MAX_CHORD_KEYS; i++) {
        keycodes[i] = keys[i];
    }

    send_keyboard_report(modifiers, keycodes);
    vTaskDelay(pdMS_TO_TICKS(TD_PRESS_HOLD_MS));
    release_all_keys();
    vTaskDelay(pdMS_TO_TICKS(TD_INTER_KEY_MS));
}

static void run_consumer(uint16_t usage)
{
    if (!wait_ready()) {
        return;
    }
    tud_hid_n_report(TD_HID_ITF_KEYBOARD, TD_REPORT_ID_CONSUMER, &usage, sizeof(usage));
    vTaskDelay(pdMS_TO_TICKS(TD_PRESS_HOLD_MS));

    uint16_t release = 0;
    if (wait_ready()) {
        tud_hid_n_report(TD_HID_ITF_KEYBOARD, TD_REPORT_ID_CONSUMER, &release, sizeof(release));
    }
    vTaskDelay(pdMS_TO_TICKS(TD_INTER_KEY_MS));
}

static void run_text(const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        uint8_t c = (uint8_t)*p;
        if (c >= 128) {
            continue;  /* ASCII only: non-ASCII needs a layout aware path */
        }
        uint8_t modifiers = k_ascii_to_keycode[c][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
        uint8_t keycode = k_ascii_to_keycode[c][1];
        if (keycode == 0) {
            continue;
        }
        run_chord(modifiers, &keycode, 1);
    }
}

static void hid_task(void *arg)
{
    (void)arg;
    td_hid_job_t job;

    while (true) {
        if (xQueueReceive(s_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!tud_mounted()) {
            ESP_LOGW(TAG, "host not connected, job type %d dropped", (int)job.type);
            continue;
        }

        switch (job.type) {
        case TD_JOB_CHORD:
            run_chord(job.chord.modifiers, job.chord.keys, job.chord.key_count);
            break;
        case TD_JOB_CONSUMER:
            run_consumer(job.consumer.usage);
            break;
        case TD_JOB_TEXT:
            run_text(job.text.text);
            break;
        case TD_JOB_DELAY:
            vTaskDelay(pdMS_TO_TICKS(job.delay.ms));
            break;
        }
    }
}

esp_err_t td_hid_start(void)
{
    if (s_queue != NULL) {
        return ESP_OK;
    }
    s_queue = xQueueCreate(TD_QUEUE_DEPTH, sizeof(td_hid_job_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    /* Pinned to core 1 so LVGL rendering on core 0 cannot delay key output. */
    if (xTaskCreatePinnedToCore(hid_task, "td_hid", 4096, NULL, 5, NULL, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t td_hid_submit(const td_hid_job_t *job)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!tud_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }
    return (xQueueSend(s_queue, job, 0) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t td_hid_send_chord(uint8_t modifiers, const uint8_t *keys, uint8_t key_count)
{
    td_hid_job_t job = {.type = TD_JOB_CHORD};
    job.chord.modifiers = modifiers;
    job.chord.key_count = (key_count > TD_MAX_CHORD_KEYS) ? TD_MAX_CHORD_KEYS : key_count;
    for (uint8_t i = 0; i < job.chord.key_count; i++) {
        job.chord.keys[i] = keys[i];
    }
    return td_hid_submit(&job);
}

esp_err_t td_hid_send_consumer(uint16_t usage)
{
    td_hid_job_t job = {.type = TD_JOB_CONSUMER};
    job.consumer.usage = usage;
    return td_hid_submit(&job);
}

esp_err_t td_hid_send_text(const char *text)
{
    td_hid_job_t job = {.type = TD_JOB_TEXT};
    strlcpy(job.text.text, text, sizeof(job.text.text));
    return td_hid_submit(&job);
}

/* ------------------------------------------------------------ TinyUSB callbacks */

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

/* Raw HID traffic from the host agent lands here; wired up in phase 3. */
__attribute__((weak)) void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                                 hid_report_type_t report_type,
                                                 uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
