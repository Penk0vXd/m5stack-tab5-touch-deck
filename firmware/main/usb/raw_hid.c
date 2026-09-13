#include "usb/raw_hid.h"

#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "tusb.h"
#include "class/hid/hid_device.h"

#include "usb/usb_descriptors.h"

static const char *TAG = "td_raw";

#define TD_HEADER_SIZE  3
#define TD_MAX_PAYLOAD  (TD_RAW_PACKET_SIZE - TD_HEADER_SIZE)
#define TD_AGENT_TIMEOUT_US (10 * 1000 * 1000)

static td_ack_cb_t s_on_ack;
static td_telemetry_cb_t s_on_telemetry;
static td_profile_cb_t s_on_profile;
static td_config_rx_cb_t s_on_config_rx;
static uint8_t s_seq;
static _Atomic int64_t s_last_agent_packet_us;

static esp_err_t send_packet(uint8_t type, uint8_t seq, const void *payload, size_t len)
{
    if (!tud_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > TD_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!tud_hid_n_ready(TD_HID_ITF_RAW)) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t packet[TD_RAW_PACKET_SIZE] = {0};
    packet[0] = type;
    packet[1] = seq;
    packet[2] = (uint8_t)len;
    if (payload != NULL && len > 0) {
        memcpy(&packet[TD_HEADER_SIZE], payload, len);
    }

    /* Report ID 0: the raw interface has no report IDs in its descriptor. */
    return tud_hid_n_report(TD_HID_ITF_RAW, 0, packet, sizeof(packet)) ? ESP_OK : ESP_FAIL;
}

esp_err_t td_raw_hid_start(void)
{
    s_seq = 0;
    atomic_store(&s_last_agent_packet_us, 0);
    const char hello[] = "tab5-touchdeck/1";
    return send_packet(TD_MSG_HELLO, s_seq++, hello, strlen(hello));
}

void td_raw_hid_set_callbacks(td_ack_cb_t on_ack, td_telemetry_cb_t on_telemetry,
                              td_profile_cb_t on_profile)
{
    s_on_ack = on_ack;
    s_on_telemetry = on_telemetry;
    s_on_profile = on_profile;
}

void td_raw_hid_set_config_rx(td_config_rx_cb_t callback)
{
    s_on_config_rx = callback;
}

esp_err_t td_raw_hid_send_ack(uint8_t seq, uint8_t status, const char *detail)
{
    uint8_t payload[TD_MAX_PAYLOAD] = {0};
    payload[0] = status;
    const size_t detail_len = (detail != NULL) ? strnlen(detail, TD_MAX_PAYLOAD - 1) : 0;
    if (detail_len > 0) {
        memcpy(&payload[1], detail, detail_len);
    }
    return send_packet(TD_MSG_ACK, seq, payload, detail_len + 1);
}

esp_err_t td_raw_hid_send_command(const char *command, uint8_t *out_seq)
{
    if (command == NULL || command[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t seq = s_seq++;
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return send_packet(TD_MSG_CMD, seq, command, strnlen(command, TD_MAX_PAYLOAD));
}

bool td_raw_hid_agent_present(void)
{
    const int64_t last_packet_us = atomic_load(&s_last_agent_packet_us);
    if (last_packet_us == 0) {
        return false;
    }
    return (esp_timer_get_time() - last_packet_us) < TD_AGENT_TIMEOUT_US;
}

static void handle_telemetry(const uint8_t *payload, uint8_t len)
{
    /* Payload is "cpu;mem;disk" as decimal text. */
    char buffer[TD_MAX_PAYLOAD + 1] = {0};
    memcpy(buffer, payload, len > TD_MAX_PAYLOAD ? TD_MAX_PAYLOAD : len);

    unsigned cpu = 0, mem = 0, disk = 0;
    if (sscanf(buffer, "%u;%u;%u", &cpu, &mem, &disk) != 3) {
        ESP_LOGW(TAG, "malformed telemetry '%s'", buffer);
        return;
    }
    if (s_on_telemetry != NULL) {
        const td_telemetry_t telemetry = {
            .cpu_percent = (uint8_t)(cpu > 100 ? 100 : cpu),
            .mem_percent = (uint8_t)(mem > 100 ? 100 : mem),
            .disk_percent = (uint8_t)(disk > 100 ? 100 : disk),
            .valid = true,
        };
        s_on_telemetry(&telemetry);
    }
}

/* Strong definition; overrides the weak stub in hid_keyboard.c. */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)report_id;
    (void)report_type;

    if (instance != TD_HID_ITF_RAW || bufsize < TD_HEADER_SIZE) {
        return;
    }

    atomic_store(&s_last_agent_packet_us, esp_timer_get_time());

    const uint8_t type = buffer[0];
    const uint8_t seq = buffer[1];
    uint8_t len = buffer[2];
    if (len > bufsize - TD_HEADER_SIZE) {
        len = (uint8_t)(bufsize - TD_HEADER_SIZE);
    }
    const uint8_t *payload = &buffer[TD_HEADER_SIZE];

    switch (type) {
    case TD_MSG_ACK: {
        if (len < 1) {
            return;
        }
        char detail[TD_MAX_PAYLOAD] = {0};
        memcpy(detail, &payload[1], len - 1);
        if (s_on_ack != NULL) {
            s_on_ack(seq, payload[0], detail);
        }
        break;
    }
    case TD_MSG_TELEMETRY:
        handle_telemetry(payload, len);
        break;
    case TD_MSG_PROFILE: {
        char profile[TD_MAX_PAYLOAD + 1] = {0};
        memcpy(profile, payload, len);
        if (s_on_profile != NULL) {
            s_on_profile(profile);
        }
        break;
    }
    case TD_MSG_PING:
        /* The host initiates probes; only the device echoes them. */
        send_packet(TD_MSG_PING, seq, NULL, 0);
        break;
    case TD_MSG_CFG_BEGIN:
    case TD_MSG_CFG_CHUNK:
    case TD_MSG_CFG_END:
        /* Writing to flash here would block the USB task; the receiver queues. */
        if (s_on_config_rx != NULL) {
            s_on_config_rx(type, seq, payload, len);
        }
        break;
    default:
        ESP_LOGD(TAG, "unhandled message type 0x%02x", type);
        break;
    }
}
