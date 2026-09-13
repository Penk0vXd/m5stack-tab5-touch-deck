/*
 * USB descriptors for Touch Deck.
 *
 * Two separate HID interfaces on purpose:
 *   itf 0 - keyboard + consumer control. Windows grabs this one exclusively
 *           with its own kbdhid driver.
 *   itf 1 - vendor defined raw HID (usage page 0xFF00). Stays open for hidapi,
 *           so the host agent can talk to the device without a driver.
 *
 * Putting both into one interface makes Windows lock the whole interface and
 * the agent channel becomes unusable.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HID instance indexes, as passed to tud_hid_n_*() */
#define TD_HID_ITF_KEYBOARD 0
#define TD_HID_ITF_RAW      1

/* Report IDs used on the keyboard interface */
#define TD_REPORT_ID_KEYBOARD 1
#define TD_REPORT_ID_CONSUMER 2

/* Raw HID packet size. Matches CFG_TUD_HID_EP_BUFSIZE. */
#define TD_RAW_PACKET_SIZE 64

/* Descriptor blobs consumed by tinyusb_driver_install() */
extern const uint8_t *td_usb_device_descriptor(void);
extern const uint8_t *td_usb_configuration_descriptor(void);
extern const char **td_usb_string_descriptors(int *count);

/*
 * ESP32-P4's USB OTG is High-Speed capable, so TinyUSB (TUD_OPT_HIGH_SPEED)
 * requires a High-Speed configuration descriptor and a device qualifier in
 * addition to the Full-Speed one, or tinyusb_driver_install() refuses with
 * ESP_ERR_INVALID_ARG. All our endpoints use 64-byte packets, which is valid
 * at both speeds, so the same bytes are reused for both.
 */
extern const void *td_usb_qualifier_descriptor(void);

#ifdef __cplusplus
}
#endif
