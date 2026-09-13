#include "ui/feedback.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "td_snd";

#define TD_SAMPLE_RATE 16000
#define TD_CHANNELS    2
#define TD_CLICK_MS    12
#define TD_CLICK_HZ    2200
#define TD_ERROR_MS    90
#define TD_ERROR_HZ    320
#define TD_VOLUME      55

typedef enum {
    TD_SOUND_CLICK = 0,
    TD_SOUND_ERROR,
} td_sound_t;

static esp_codec_dev_handle_t s_speaker;
static QueueHandle_t s_queue;
static int16_t *s_click;
static size_t s_click_bytes;
static int16_t *s_error;
static size_t s_error_bytes;

/* Renders a sine burst with a short fade so it does not pop. */
static int16_t *render_tone(int freq_hz, int duration_ms, size_t *out_bytes)
{
    const size_t frames = (size_t)TD_SAMPLE_RATE * duration_ms / 1000;
    const size_t samples = frames * TD_CHANNELS;
    int16_t *buffer = calloc(samples, sizeof(int16_t));
    if (buffer == NULL) {
        return NULL;
    }

    const size_t fade = frames / 4;
    for (size_t i = 0; i < frames; i++) {
        float envelope = 1.0f;
        if (i < fade) {
            envelope = (float)i / (float)fade;
        } else if (i > frames - fade) {
            envelope = (float)(frames - i) / (float)fade;
        }
        const float phase = 2.0f * (float)M_PI * (float)freq_hz * (float)i / (float)TD_SAMPLE_RATE;
        const int16_t value = (int16_t)(sinf(phase) * envelope * 9000.0f);
        buffer[i * TD_CHANNELS] = value;
        buffer[i * TD_CHANNELS + 1] = value;
    }

    *out_bytes = samples * sizeof(int16_t);
    return buffer;
}

static void audio_task(void *arg)
{
    (void)arg;
    td_sound_t sound;

    while (true) {
        if (xQueueReceive(s_queue, &sound, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const int16_t *data = (sound == TD_SOUND_ERROR) ? s_error : s_click;
        const size_t bytes = (sound == TD_SOUND_ERROR) ? s_error_bytes : s_click_bytes;
        if (data == NULL || s_speaker == NULL) {
            continue;
        }
        esp_codec_dev_write(s_speaker, (void *)data, bytes);
    }
}

esp_err_t td_feedback_init(void)
{
    esp_err_t err = bsp_audio_init(NULL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "audio init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_speaker = bsp_audio_codec_speaker_init();
    if (s_speaker == NULL) {
        ESP_LOGW(TAG, "speaker codec unavailable, touch feedback disabled");
        return ESP_ERR_NOT_FOUND;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = TD_SAMPLE_RATE,
        .channel = TD_CHANNELS,
        .bits_per_sample = 16,
    };
    err = esp_codec_dev_open(s_speaker, &fs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "codec open failed: %s", esp_err_to_name(err));
        s_speaker = NULL;
        return err;
    }
    esp_codec_dev_set_out_vol(s_speaker, TD_VOLUME);

    s_click = render_tone(TD_CLICK_HZ, TD_CLICK_MS, &s_click_bytes);
    s_error = render_tone(TD_ERROR_HZ, TD_ERROR_MS, &s_error_bytes);
    if (s_click == NULL || s_error == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_queue = xQueueCreate(4, sizeof(td_sound_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(audio_task, "td_snd", 3072, NULL, 4, NULL, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "touch feedback ready");
    return ESP_OK;
}

bool td_feedback_available(void)
{
    return s_speaker != NULL && s_queue != NULL;
}

static void enqueue(td_sound_t sound)
{
    if (s_queue == NULL) {
        return;
    }
    xQueueSend(s_queue, &sound, 0);
}

void td_feedback_click(void)
{
    enqueue(TD_SOUND_CLICK);
}

void td_feedback_error(void)
{
    enqueue(TD_SOUND_ERROR);
}
