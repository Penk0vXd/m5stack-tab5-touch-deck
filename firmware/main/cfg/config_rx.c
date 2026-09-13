#include "cfg/config_rx.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "cfg/config.h"
#include "usb/raw_hid.h"

static const char *TAG = "td_cfgrx";

#define TD_RX_QUEUE_DEPTH 24
#define TD_RX_PAYLOAD_MAX 61
#define TD_TMP_SUFFIX     ".tmp"

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t data[TD_RX_PAYLOAD_MAX];
} td_rx_packet_t;

static QueueHandle_t s_queue;
static char s_config_path[64];
static char s_tmp_path[72];
static td_config_applied_cb_t s_on_applied;
static FILE *s_file;
static size_t s_received;

static void abort_upload(const char *reason)
{
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    unlink(s_tmp_path);
    s_received = 0;
    ESP_LOGW(TAG, "upload aborted: %s", reason);
    td_raw_hid_send_ack(0, TD_ACK_FAILED, reason);
}

static void handle_begin(void)
{
    if (s_file != NULL) {
        fclose(s_file);
    }
    s_received = 0;
    s_file = fopen(s_tmp_path, "wb");
    if (s_file == NULL) {
        abort_upload("cannot open temp file");
        return;
    }
    ESP_LOGI(TAG, "config upload started");
}

static void handle_chunk(const td_rx_packet_t *packet)
{
    if (s_file == NULL) {
        return;  /* CHUNK without BEGIN: ignore quietly, the agent retries */
    }
    if (fwrite(packet->data, 1, packet->len, s_file) != packet->len) {
        abort_upload("write failed");
        return;
    }
    s_received += packet->len;
}

static void handle_end(void)
{
    if (s_file == NULL) {
        return;
    }
    fclose(s_file);
    s_file = NULL;

    /* Parse the upload before it replaces a working config. */
    td_config_t *probe = td_config_alloc();
    if (probe == NULL) {
        abort_upload("out of memory");
        return;
    }
    char err[64] = {0};
    const esp_err_t parsed = td_config_load(s_tmp_path, probe, err, sizeof(err));
    td_config_free(probe);
    if (parsed != ESP_OK) {
        abort_upload(err[0] != '\0' ? err : "invalid config");
        return;
    }

    unlink(s_config_path);
    if (rename(s_tmp_path, s_config_path) != 0) {
        abort_upload("rename failed");
        return;
    }

    ESP_LOGI(TAG, "config replaced (%u bytes)", (unsigned)s_received);
    td_raw_hid_send_ack(0, TD_ACK_OK, "config applied");
    if (s_on_applied != NULL) {
        s_on_applied();
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    td_rx_packet_t packet;

    while (true) {
        if (xQueueReceive(s_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (packet.type) {
        case TD_MSG_CFG_BEGIN:
            handle_begin();
            break;
        case TD_MSG_CFG_CHUNK:
            handle_chunk(&packet);
            break;
        case TD_MSG_CFG_END:
            handle_end();
            break;
        default:
            break;
        }
    }
}

/* Runs in the USB task: copy and hand over, nothing more. */
static void on_config_packet(uint8_t msg_type, const uint8_t *payload, uint8_t len)
{
    if (s_queue == NULL) {
        return;
    }
    td_rx_packet_t packet = {.type = msg_type, .len = len > TD_RX_PAYLOAD_MAX ? TD_RX_PAYLOAD_MAX : len};
    if (payload != NULL && packet.len > 0) {
        memcpy(packet.data, payload, packet.len);
    }
    xQueueSend(s_queue, &packet, 0);
}

esp_err_t td_config_rx_start(const char *config_path, td_config_applied_cb_t on_applied)
{
    if (config_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(s_config_path, config_path, sizeof(s_config_path));
    snprintf(s_tmp_path, sizeof(s_tmp_path), "%s%s", config_path, TD_TMP_SUFFIX);
    s_on_applied = on_applied;

    s_queue = xQueueCreate(TD_RX_QUEUE_DEPTH, sizeof(td_rx_packet_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(rx_task, "td_cfgrx", 5120, NULL, 4, NULL, 0) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    td_raw_hid_set_config_rx(on_config_packet);
    return ESP_OK;
}
