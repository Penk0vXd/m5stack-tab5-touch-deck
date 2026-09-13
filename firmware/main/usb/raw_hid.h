/*
 * Vendor raw HID channel to the host agent.
 *
 * Packet layout (64 bytes, mirrored in host-agent/protocol.py):
 *   byte 0      message type
 *   byte 1      sequence number
 *   byte 2      payload length (0..61)
 *   bytes 3..63 payload
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TD_MSG_HELLO     0x01
#define TD_MSG_CMD       0x02
#define TD_MSG_ACK       0x03
#define TD_MSG_TELEMETRY 0x04
#define TD_MSG_PROFILE   0x05
#define TD_MSG_PING      0x06
/* Configuration upload: BEGIN, then CHUNK packets in order, then END. */
#define TD_MSG_CFG_BEGIN 0x07
#define TD_MSG_CFG_CHUNK 0x08
#define TD_MSG_CFG_END   0x09

#define TD_ACK_OK              0x00
#define TD_ACK_UNKNOWN_COMMAND 0x01
#define TD_ACK_NOT_ALLOWED     0x02
#define TD_ACK_FAILED          0x03

typedef struct {
    uint8_t cpu_percent;
    uint8_t mem_percent;
    uint8_t disk_percent;
    bool valid;
} td_telemetry_t;

/* Called from the USB task; keep implementations short and non-blocking. */
typedef void (*td_ack_cb_t)(uint8_t seq, uint8_t status, const char *detail);
typedef void (*td_telemetry_cb_t)(const td_telemetry_t *telemetry);
typedef void (*td_profile_cb_t)(const char *profile);

/* Config upload events, forwarded to cfg/config_rx. */
typedef void (*td_config_rx_cb_t)(uint8_t msg_type, const uint8_t *payload, uint8_t len);
void td_raw_hid_set_config_rx(td_config_rx_cb_t callback);

/* Replies to a config upload so the agent knows whether it took. */
esp_err_t td_raw_hid_send_ack(uint8_t seq, uint8_t status, const char *detail);

esp_err_t td_raw_hid_start(void);
void td_raw_hid_set_callbacks(td_ack_cb_t on_ack, td_telemetry_cb_t on_telemetry,
                              td_profile_cb_t on_profile);

/* Asks the agent to run a named command. Returns the sequence number used. */
esp_err_t td_raw_hid_send_command(const char *command, uint8_t *out_seq);

/* True when an agent has answered at least one packet recently. */
bool td_raw_hid_agent_present(void);

#ifdef __cplusplus
}
#endif
