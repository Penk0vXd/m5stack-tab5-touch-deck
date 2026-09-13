/*
 * Tab5 Touch Deck - firmware entry point.
 *
 * Boot order matters: display first so the user sees something while USB
 * enumerates, then storage and config, then TinyUSB, then the UI.
 */
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "tinyusb.h"

#include "cfg/config.h"
#include "cfg/config_rx.h"
#include "ui/deck_ui.h"
#include "ui/feedback.h"
#include "ui/imu_gestures.h"
#include "usb/hid_keyboard.h"
#include "usb/keymap.h"
#include "usb/raw_hid.h"
#include "usb/usb_descriptors.h"

static const char *TAG = "touchdeck";

#define TD_CONFIG_PATH CONFIG_BSP_SPIFFS_MOUNT_POINT "/config.json"

static td_config_t *s_config;
static char s_config_error[96];

static void usb_init(void)
{
    /*
     * The Type-C port's device-mode routing (and possibly its D+ pull-up)
     * appears to be gated by the same IO-expander bit the BSP's USB Host
     * path uses (BSP_USB_EN). Without this, TinyUSB installs cleanly in
     * firmware but the host never sees an attach - matches the M5Stack
     * community reports of "no HID output from Tab5 over USB-C".
     */
    esp_err_t usb_power_err = bsp_feature_enable(BSP_FEATURE_USB, true);
    if (usb_power_err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_feature_enable(BSP_FEATURE_USB) failed: %s",
                 esp_err_to_name(usb_power_err));
    }

    int string_count = 0;
    const char **strings = td_usb_string_descriptors(&string_count);

    /*
     * On the High-Speed (OTG2.0/UTMI) rhport, TinyUSB (TUD_OPT_HIGH_SPEED)
     * requires both a Full-Speed and a High-Speed configuration descriptor
     * plus a device qualifier, or it refuses to install; the Full-Speed
     * (OTG1.1) rhport uses the plain single-descriptor field instead. All
     * endpoints here use 64-byte packets, valid at either speed.
     */
#if CONFIG_TINYUSB_RHPORT_HS
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = (const tusb_desc_device_t *)td_usb_device_descriptor(),
        .string_descriptor = strings,
        .string_descriptor_count = string_count,
        .external_phy = false,
        .fs_configuration_descriptor = td_usb_configuration_descriptor(),
        .hs_configuration_descriptor = td_usb_configuration_descriptor(),
        .qualifier_descriptor = (const tusb_desc_device_qualifier_t *)td_usb_qualifier_descriptor(),
    };
#else
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = (const tusb_desc_device_t *)td_usb_device_descriptor(),
        .string_descriptor = strings,
        .string_descriptor_count = string_count,
        .external_phy = false,
        .configuration_descriptor = td_usb_configuration_descriptor(),
    };
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "TinyUSB installed: HID keyboard + raw HID");
}

/* Raw HID callbacks run in the USB task and only hand state to the UI layer. */
static void on_ack(uint8_t seq, uint8_t status, const char *detail)
{
    td_ui_on_ack(seq, status, detail);
}

static void on_telemetry(const td_telemetry_t *telemetry)
{
    td_ui_on_telemetry(telemetry);
}

static void on_profile(const char *profile)
{
    td_ui_on_profile(profile);
}

static void on_config_applied(void)
{
    td_ui_reload_from_file(TD_CONFIG_PATH);
}

static const td_config_t *load_config(void)
{
    esp_err_t err = bsp_spiffs_mount();
    if (err != ESP_OK) {
        snprintf(s_config_error, sizeof(s_config_error), "storage mount failed: %s",
                 esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", s_config_error);
        return NULL;
    }

    s_config = td_config_alloc();
    if (s_config == NULL) {
        snprintf(s_config_error, sizeof(s_config_error), "out of memory for config model");
        return NULL;
    }

    err = td_config_load(TD_CONFIG_PATH, s_config, s_config_error, sizeof(s_config_error));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config load failed: %s", s_config_error);
        td_config_free(s_config);
        s_config = NULL;
        return NULL;
    }
    return s_config;
}

void app_main(void)
{
    /* Panel is 720x1280 portrait; the deck runs landscape. */
    lv_display_t *display = bsp_display_start();
    bsp_display_rotate(display, LV_DISPLAY_ROTATION_90);
    lv_indev_t *touch = bsp_display_get_input_dev();
    if (touch != NULL) {
        /* Polling is more robust than the ST712x interrupt line after USB resets. */
        lv_indev_set_mode(touch, LV_INDEV_MODE_TIMER);
    }
    bsp_display_backlight_on();

    td_keymap_set_platform(TD_PLATFORM_WINDOWS);
    const td_config_t *config = load_config();

    /* Touch feedback is optional; a missing codec must not stop the deck. */
    if (td_feedback_init() != ESP_OK) {
        ESP_LOGW(TAG, "continuing without audio feedback");
    }

    usb_init();
    ESP_ERROR_CHECK(td_hid_start());
    td_raw_hid_set_callbacks(on_ack, on_telemetry, on_profile);
    td_raw_hid_start();

    ESP_ERROR_CHECK(td_ui_start(config, config == NULL ? s_config_error : NULL));

    /* Config uploads arrive over the same raw HID channel as commands. */
    if (td_config_rx_start(TD_CONFIG_PATH, on_config_applied) != ESP_OK) {
        ESP_LOGW(TAG, "config upload channel unavailable");
    }

    /* Gestures are a convenience: a missing IMU must not stop the deck. */
    if (td_imu_start(td_ui_on_gesture) != ESP_OK) {
        ESP_LOGW(TAG, "continuing without tilt gestures");
    }

    ESP_LOGI(TAG, "ready");
}
