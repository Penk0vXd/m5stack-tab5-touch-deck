#include "cfg/config_rx.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "cfg/config.h"
#include "usb/raw_hid.h"

static const char *TAG = "td_cfgrx";

#define TD_RX_QUEUE_DEPTH 24
#define TD_RX_PAYLOAD_MAX 61
#define TD_TMP_SUFFIX     ".tmp"
#define TD_BACKUP_SUFFIX  ".bak"
#define TD_MAX_CONFIG_BYTES (64 * 1024)
#define TD_UPLOAD_TIMEOUT_US (5 * 1000 * 1000)

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t len;
    uint8_t data[TD_RX_PAYLOAD_MAX];
} td_rx_packet_t;

static QueueHandle_t s_queue;
static char s_config_path[64];
static char s_tmp_path[72];
static char s_backup_path[72];
static td_config_applied_cb_t s_on_applied;
static FILE *s_file;
static size_t s_received;
static uint8_t s_expected_seq;
static uint8_t s_last_seq;
static int64_t s_last_packet_us;

static void abort_upload(uint8_t seq, const char *reason)
{
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    unlink(s_tmp_path);
    s_received = 0;
    s_last_packet_us = 0;
    ESP_LOGW(TAG, "upload aborted: %s", reason);
    td_raw_hid_send_ack(seq, TD_ACK_FAILED, reason);
}

static void handle_begin(const td_rx_packet_t *packet)
{
    if (s_file != NULL) {
        fclose(s_file);
        unlink(s_tmp_path);
    }
    s_received = 0;
    s_last_seq = packet->seq;
    s_expected_seq = (uint8_t)(packet->seq + 1);
    s_last_packet_us = esp_timer_get_time();
    s_file = fopen(s_tmp_path, "wb");
    if (s_file == NULL) {
        abort_upload(packet->seq, "cannot open temp file");
        return;
    }
    ESP_LOGI(TAG, "config upload started");
}

static void handle_chunk(const td_rx_packet_t *packet)
{
    if (s_file == NULL) {
        td_raw_hid_send_ack(packet->seq, TD_ACK_FAILED, "upload not started");
        return;
    }
    if (packet->seq != s_expected_seq) {
        abort_upload(packet->seq, "upload sequence error");
        return;
    }
    if (s_received + packet->len > TD_MAX_CONFIG_BYTES) {
        abort_upload(packet->seq, "config too large");
        return;
    }
    if (fwrite(packet->data, 1, packet->len, s_file) != packet->len) {
        abort_upload(packet->seq, "write failed");
        return;
    }
    s_received += packet->len;
    s_last_seq = packet->seq;
    s_expected_seq = (uint8_t)(packet->seq + 1);
    s_last_packet_us = esp_timer_get_time();
}

static void handle_end(const td_rx_packet_t *packet)
{
    if (s_file == NULL) {
        td_raw_hid_send_ack(packet->seq, TD_ACK_FAILED, "upload not started");
        return;
    }
    if (packet->seq != s_expected_seq) {
        abort_upload(packet->seq, "upload sequence error");
        return;
    }
    fclose(s_file);
    s_file = NULL;

    /* Parse the upload before it replaces a working config. */
    td_config_t *probe = td_config_alloc();
    if (probe == NULL) {
        abort_upload(packet->seq, "out of memory");
        return;
    }
    char err[64] = {0};
    const esp_err_t parsed = td_config_load(s_tmp_path, probe, err, sizeof(err));
    td_config_free(probe);
    if (parsed != ESP_OK) {
        abort_upload(packet->seq, err[0] != '\0' ? err : "invalid config");
        return;
    }

    const bool had_config = access(s_config_path, F_OK) == 0;
    unlink(s_backup_path);
    if (had_config && rename(s_config_path, s_backup_path) != 0) {
        abort_upload(packet->seq, "backup failed");
        return;
    }
    if (rename(s_tmp_path, s_config_path) != 0) {
        if (had_config) {
            rename(s_backup_path, s_config_path);
        }
        abort_upload(packet->seq, "replace failed");
        return;
    }

    esp_err_t applied = ESP_OK;
    if (s_on_applied != NULL) {
        applied = s_on_applied();
    }
    if (applied != ESP_OK) {
        unlink(s_config_path);
        if (had_config) {
            rename(s_backup_path, s_config_path);
            if (s_on_applied != NULL) {
                s_on_applied();
            }
        }
        s_received = 0;
        s_last_packet_us = 0;
        td_raw_hid_send_ack(packet->seq, TD_ACK_FAILED, "UI reload failed");
        return;
    }
    unlink(s_backup_path);
    ESP_LOGI(TAG, "config replaced (%u bytes)", (unsigned)s_received);
    td_raw_hid_send_ack(packet->seq, TD_ACK_OK, "config applied");
    s_received = 0;
    s_last_packet_us = 0;
}

static void rx_task(void *arg)
{
    (void)arg;
    td_rx_packet_t packet;

    while (true) {
        if (xQueueReceive(s_queue, &packet, pdMS_TO_TICKS(1000)) != pdTRUE) {
            if (s_file != NULL && s_last_packet_us != 0 &&
                esp_timer_get_time() - s_last_packet_us > TD_UPLOAD_TIMEOUT_US) {
                abort_upload(s_last_seq, "upload timed out");
            }
            continue;
        }
        switch (packet.type) {
        case TD_MSG_CFG_BEGIN:
            handle_begin(&packet);
            break;
        case TD_MSG_CFG_CHUNK:
            handle_chunk(&packet);
            break;
        case TD_MSG_CFG_END:
            handle_end(&packet);
            break;
        default:
            break;
        }
    }
}

/* Runs in the USB task: copy and hand over, nothing more. */
static void on_config_packet(uint8_t msg_type, uint8_t seq, const uint8_t *payload, uint8_t len)
{
    if (s_queue == NULL) {
        return;
    }
    td_rx_packet_t packet = {
        .type = msg_type,
        .seq = seq,
        .len = len > TD_RX_PAYLOAD_MAX ? TD_RX_PAYLOAD_MAX : len,
    };
    if (payload != NULL && packet.len > 0) {
        memcpy(packet.data, payload, packet.len);
    }
    if (xQueueSend(s_queue, &packet, 0) != pdTRUE) {
        ESP_LOGW(TAG, "upload queue full at sequence %u", seq);
        td_raw_hid_send_ack(seq, TD_ACK_FAILED, "upload busy");
    }
}

esp_err_t td_config_rx_start(const char *config_path, td_config_applied_cb_t on_applied)
{
    if (config_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(s_config_path, config_path, sizeof(s_config_path));
    snprintf(s_tmp_path, sizeof(s_tmp_path), "%s%s", config_path, TD_TMP_SUFFIX);
    snprintf(s_backup_path, sizeof(s_backup_path), "%s%s", config_path, TD_BACKUP_SUFFIX);
    s_on_applied = on_applied;

    s_queue = xQueueCreate(TD_RX_QUEUE_DEPTH, sizeof(td_rx_packet_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(rx_task, "td_cfgrx", 5120, NULL, 4, NULL, 0) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    td_raw_hid_set_config_rx(on_config_packet);
    return ESP_OK;
}
