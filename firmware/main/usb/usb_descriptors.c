#include "usb/usb_descriptors.h"

#include "tusb.h"
#include "class/hid/hid_device.h"

/* Espressif VID with a development PID. Replace before distributing anything. */
#define TD_USB_VID 0x303A
#define TD_USB_PID 0x4004

enum {
    ITF_NUM_HID_KEYBOARD = 0,
    ITF_NUM_HID_RAW,
    ITF_NUM_TOTAL,
};

enum {
    EPNUM_HID_KEYBOARD = 0x81,
    EPNUM_HID_RAW_OUT  = 0x02,
    EPNUM_HID_RAW_IN   = 0x82,
};

/* ---------------------------------------------------------------- report descriptors */

static const uint8_t desc_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(TD_REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(TD_REPORT_ID_CONSUMER)),
};

static const uint8_t desc_hid_report_raw[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(TD_RAW_PACKET_SIZE),
};

/* TinyUSB asks per instance which report descriptor to hand out. */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return (instance == TD_HID_ITF_RAW) ? desc_hid_report_raw : desc_hid_report_keyboard;
}

/* ---------------------------------------------------------------- device descriptor */

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = TD_USB_VID,
    .idProduct          = TD_USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

/* ---------------------------------------------------------------- configuration */

#define TD_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

/*
 * bInterval 4: on a high speed port that is 2^(4-1) microframes = 1 ms, which is
 * the fastest polling a HID interface is allowed to ask for.
 */
static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TD_CONFIG_TOTAL_LEN, 0x00, 100),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, 4, false, sizeof(desc_hid_report_keyboard),
                       EPNUM_HID_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, 4),

    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID_RAW, 5, false, sizeof(desc_hid_report_raw),
                             EPNUM_HID_RAW_OUT, EPNUM_HID_RAW_IN, CFG_TUD_HID_EP_BUFSIZE, 4),
};

/* ---------------------------------------------------------------- strings */

static const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04},  /* 0: English (0x0409) */
    "M5Stack",                   /* 1: manufacturer */
    "Tab5 Touch Deck",           /* 2: product */
    "TD0001",                    /* 3: serial */
    "Touch Deck Keyboard",       /* 4: HID keyboard interface */
    "Touch Deck Control",        /* 5: raw HID interface */
};

const uint8_t *td_usb_device_descriptor(void)
{
    return (const uint8_t *)&desc_device;
}

const uint8_t *td_usb_configuration_descriptor(void)
{
    return desc_configuration;
}

/* Device qualifier: tells the host what the device would look like at the
 * other USB speed. Values mirror desc_device; num_configurations is fixed
 * at 1 per the USB 2.0 spec for this descriptor type. */
static const tusb_desc_device_qualifier_t desc_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00,
};

const void *td_usb_qualifier_descriptor(void)
{
    return &desc_qualifier;
}

const char **td_usb_string_descriptors(int *count)
{
    *count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
    return (const char **)string_desc_arr;
}
