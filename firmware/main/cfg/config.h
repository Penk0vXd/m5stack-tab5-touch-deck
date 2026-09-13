/*
 * Deck configuration model.
 *
 * config.json lives on the "storage" partition and is parsed once at boot into
 * this model. Parsing failures are never fatal: the caller falls back to the
 * built-in deck and shows the error on screen.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "usb/keymap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TD_CFG_MAX_PAGES        8
#define TD_CFG_MAX_BUTTONS      20
#define TD_CFG_MAX_MACRO_STEPS  8
#define TD_CFG_MAX_KEYS         4
#define TD_CFG_LABEL_LEN        24
#define TD_CFG_ID_LEN           16
#define TD_CFG_TEXT_LEN         96
#define TD_CFG_COMMAND_LEN      32
#define TD_CFG_MAX_MATCH        4
#define TD_CFG_MATCH_LEN        24

typedef enum {
    TD_ACTION_NONE = 0,
    TD_ACTION_HID_KEYS,
    TD_ACTION_HID_CONSUMER,
    TD_ACTION_TEXT,
    TD_ACTION_MACRO,
    TD_ACTION_AGENT,
    TD_ACTION_GOTO_PAGE,
    TD_ACTION_DELAY,
} td_action_type_t;

typedef struct {
    td_action_type_t type;
    uint8_t modifiers;                 /* HID_KEYS: resolved modifier mask */
    uint8_t keys[TD_CFG_MAX_KEYS];     /* HID_KEYS: resolved keycodes */
    uint8_t key_count;
    uint16_t consumer_usage;           /* HID_CONSUMER */
    uint32_t delay_ms;                 /* DELAY */
    char text[TD_CFG_TEXT_LEN];        /* TEXT */
    char command[TD_CFG_COMMAND_LEN];  /* AGENT: name the host agent must know */
    char page[TD_CFG_ID_LEN];          /* GOTO_PAGE */
} td_action_t;

typedef struct {
    td_action_t steps[TD_CFG_MAX_MACRO_STEPS];
    uint8_t step_count;
} td_macro_t;

typedef struct {
    bool enabled;
    uint16_t up_usage;    /* consumer usage sent when the value rises */
    uint16_t down_usage;  /* consumer usage sent when the value falls */
    uint8_t step;         /* percent per emitted step */
} td_slider_t;

#define TD_CFG_COLOR_LEN 12

typedef struct {
    char label[TD_CFG_LABEL_LEN];
    char tile[TD_CFG_ID_LEN];   /* non-empty: renders a telemetry tile instead */
    /* Accent name ("cyan", "amber", ...) or "#RRGGBB". Empty means the UI
     * picks one from the accent ramp by position. */
    char color[TD_CFG_COLOR_LEN];
    td_slider_t slider;         /* enabled: renders a slider instead */
    td_action_t action;         /* used when action.type != TD_ACTION_MACRO */
    td_macro_t macro;           /* used when action.type == TD_ACTION_MACRO */
    td_action_t long_press;     /* TD_ACTION_NONE when not configured */
    bool has_long_press;
} td_button_t;

typedef struct {
    char id[TD_CFG_ID_LEN];
    char title[TD_CFG_LABEL_LEN];
    td_button_t buttons[TD_CFG_MAX_BUTTONS];
    uint8_t button_count;
    /* Lower-case substrings of the host's active window title. First page whose
     * entry matches wins when the agent reports a profile change. */
    char match[TD_CFG_MAX_MATCH][TD_CFG_MATCH_LEN];
    uint8_t match_count;
} td_page_t;

typedef struct {
    uint8_t cols;
    uint8_t rows;
    td_platform_t platform;
    uint8_t brightness;        /* percent, active */
    uint16_t screensaver_sec;  /* 0 disables dimming */
    td_page_t pages[TD_CFG_MAX_PAGES];
    uint8_t page_count;
} td_config_t;

/*
 * The model is a few hundred kilobytes, which does not fit in internal SRAM.
 * Every instance therefore lives in PSRAM; the device has 32 MB of it.
 */
td_config_t *td_config_alloc(void);
void td_config_free(td_config_t *config);

/*
 * Mounts the storage partition and parses config_path (for example
 * "/spiffs/config.json"). On failure returns an error and fills err_msg.
 */
esp_err_t td_config_load(const char *config_path, td_config_t *out_config,
                         char *err_msg, size_t err_msg_len);

/* Index of a page by id, or -1. */
int td_config_find_page(const td_config_t *config, const char *page_id);

#ifdef __cplusplus
}
#endif
