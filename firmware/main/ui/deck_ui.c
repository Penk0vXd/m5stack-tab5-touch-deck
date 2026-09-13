#include "ui/deck_ui.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"

#include "ui/feedback.h"
#include "ui/theme.h"
#include "usb/hid_keyboard.h"
#include "usb/keymap.h"
#include "usb/raw_hid.h"

static const char *TAG = "td_ui";

#define TD_HEADER_H 64
#define TD_FEEDBACK_H 32
#define TD_NAV_H    66
#define TD_GAP      12

/* Landscape geometry: the panel is 720x1280 portrait, rotated by 90 degrees. */
#define TD_SCREEN_W BSP_LCD_V_RES
#define TD_SCREEN_H BSP_LCD_H_RES
#define TD_CONTENT_H (TD_SCREEN_H - TD_HEADER_H - TD_FEEDBACK_H - TD_NAV_H - 2 * TD_GAP)

typedef struct {
    const td_button_t *def;
    lv_obj_t *value_label;  /* telemetry tiles */
    lv_obj_t *value_bar;    /* telemetry tiles */
    lv_color_t accent;
    int16_t last_slider_value;
    bool long_press_handled;
} td_button_ctx_t;

static const td_config_t *s_config;
static td_config_t *s_fallback;   /* PSRAM: the model is too big for SRAM */
static td_config_t *s_reloaded;   /* PSRAM: destination of config uploads */
static int s_page_index;

static lv_obj_t *s_page_label;
static lv_obj_t *s_usb_pill;
static lv_obj_t *s_agent_pill;
static lv_obj_t *s_nav;
static lv_obj_t *s_toast_label;
static lv_obj_t *s_grid;
static td_button_ctx_t s_button_ctx[TD_CFG_MAX_BUTTONS];

/* Written from the USB task, read by the LVGL timer. */
static _Atomic uint32_t s_telemetry_packed;  /* cpu | mem<<8 | disk<<16 | valid<<24 */
static _Atomic int64_t s_telemetry_received_us;
static _Atomic uint32_t s_ack_packed;        /* status | seq<<8 | counter<<16 */
static char s_ack_detail[32];
static _Atomic uint32_t s_profile_counter;
static char s_profile[64];
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_dimmed;
static _Atomic uint32_t s_gesture_packed;  /* gesture | counter<<8 */

static void build_page(int page_index);

static lv_color_t page_accent(int page_index)
{
    return td_theme_accent((uint8_t)page_index);
}

static const char *icon_symbol(const char *name)
{
    if (name == NULL || name[0] == '\0') return "";
    if (strcmp(name, "home") == 0) return LV_SYMBOL_HOME;
    if (strcmp(name, "work") == 0 || strcmp(name, "list") == 0) return LV_SYMBOL_LIST;
    if (strcmp(name, "media") == 0 || strcmp(name, "audio") == 0) return LV_SYMBOL_AUDIO;
    if (strcmp(name, "settings") == 0) return LV_SYMBOL_SETTINGS;
    if (strcmp(name, "terminal") == 0 || strcmp(name, "keyboard") == 0) return LV_SYMBOL_KEYBOARD;
    if (strcmp(name, "copy") == 0) return LV_SYMBOL_COPY;
    if (strcmp(name, "paste") == 0) return LV_SYMBOL_PASTE;
    if (strcmp(name, "undo") == 0 || strcmp(name, "switch") == 0) return LV_SYMBOL_REFRESH;
    if (strcmp(name, "snip") == 0) return LV_SYMBOL_CUT;
    if (strcmp(name, "notes") == 0 || strcmp(name, "file") == 0) return LV_SYMBOL_FILE;
    if (strcmp(name, "save") == 0) return LV_SYMBOL_SAVE;
    if (strcmp(name, "play") == 0) return LV_SYMBOL_PLAY;
    if (strcmp(name, "next") == 0) return LV_SYMBOL_NEXT;
    if (strcmp(name, "back") == 0) return LV_SYMBOL_PREV;
    if (strcmp(name, "power") == 0 || strcmp(name, "lock") == 0) return LV_SYMBOL_POWER;
    if (strcmp(name, "edit") == 0) return LV_SYMBOL_EDIT;
    if (strcmp(name, "upload") == 0) return LV_SYMBOL_UPLOAD;
    if (strcmp(name, "download") == 0) return LV_SYMBOL_DOWNLOAD;
    return "";
}

/* ------------------------------------------------------------------ actions */

static void run_action(const td_action_t *action, const td_macro_t *macro);

static void run_macro(const td_macro_t *macro)
{
    for (uint8_t i = 0; i < macro->step_count; i++) {
        run_action(&macro->steps[i], NULL);
    }
}

static void toast(const char *text, lv_color_t color)
{
    lv_label_set_text(s_toast_label, text);
    lv_obj_set_style_text_color(s_toast_label, color, LV_PART_MAIN);
}

static void run_action(const td_action_t *action, const td_macro_t *macro)
{
    switch (action->type) {
    case TD_ACTION_HID_KEYS:
        if (td_hid_send_chord(action->modifiers, action->keys, action->key_count) != ESP_OK) {
            toast("keyboard queue unavailable", td_theme_danger());
        }
        break;
    case TD_ACTION_HID_CONSUMER:
        if (td_hid_send_consumer(action->consumer_usage) != ESP_OK) {
            toast("media queue unavailable", td_theme_danger());
        }
        break;
    case TD_ACTION_TEXT:
        if (td_hid_send_text(action->text) != ESP_OK) {
            toast("text queue unavailable", td_theme_danger());
        }
        break;
    case TD_ACTION_DELAY: {
        td_hid_job_t job = {.type = TD_JOB_DELAY};
        job.delay.ms = action->delay_ms;
        if (td_hid_submit(&job) != ESP_OK) {
            toast("macro queue unavailable", td_theme_danger());
        }
        break;
    }
    case TD_ACTION_BRIGHTNESS: {
        lv_display_trigger_activity(NULL);
        bsp_display_brightness_set(action->brightness);
        s_dimmed = false;
        char msg[32];
        snprintf(msg, sizeof(msg), "brightness %u%%", action->brightness);
        toast(msg, td_theme_text_dim());
        break;
    }
    case TD_ACTION_MACRO:
        if (macro != NULL) {
            run_macro(macro);
        }
        break;
    case TD_ACTION_AGENT:
        if (td_raw_hid_send_command(action->command, NULL) != ESP_OK) {
            char msg[64];
            snprintf(msg, sizeof(msg), "agent offline - %s not sent", action->command);
            toast(msg, td_theme_danger());
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "sent %s", action->command);
            toast(msg, td_theme_text_dim());
        }
        break;
    case TD_ACTION_GOTO_PAGE: {
        const int index = td_config_find_page(s_config, action->page);
        if (index >= 0) {
            build_page(index);
        } else {
            ESP_LOGW(TAG, "unknown page '%s'", action->page);
        }
        break;
    }
    case TD_ACTION_NONE:
        break;
    }
}

static void button_event_cb(lv_event_t *event)
{
    td_button_ctx_t *ctx = (td_button_ctx_t *)lv_event_get_user_data(event);
    if (ctx == NULL || ctx->def == NULL) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_LONG_PRESSED && ctx->def->has_long_press) {
        ctx->long_press_handled = true;
        td_feedback_click();
        run_action(&ctx->def->long_press, &ctx->def->long_press_macro);
        return;
    }
    if (code == LV_EVENT_CLICKED) {
        if (ctx->long_press_handled) {
            ctx->long_press_handled = false;
            return;
        }
        if (ctx->def->action.type == TD_ACTION_NONE && ctx->def->has_long_press) {
            td_feedback_error();
            toast("hold to run", td_theme_warn());
            return;
        }
        if (td_hid_is_connected()) {
            td_feedback_click();
        } else {
            td_feedback_error();
            toast("USB not connected", td_theme_warn());
        }
        run_action(&ctx->def->action, &ctx->def->macro);
    }
}

/*
 * Sliders emit consumer key steps instead of an absolute value, so they work
 * with no host agent installed. One step per `step` percent of travel.
 */
static void slider_event_cb(lv_event_t *event)
{
    td_button_ctx_t *ctx = (td_button_ctx_t *)lv_event_get_user_data(event);
    if (ctx == NULL || ctx->def == NULL || !ctx->def->slider.enabled) {
        return;
    }

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);
    const int16_t value = (int16_t)lv_slider_get_value(slider);
    const int16_t delta = value - ctx->last_slider_value;
    const uint8_t step = ctx->def->slider.step;

    int steps = delta / (int)step;
    if (steps == 0) {
        return;
    }
    /* Keep one gesture from flooding the HID queue. */
    if (steps > 8) {
        steps = 8;
    } else if (steps < -8) {
        steps = -8;
    }

    const uint16_t usage = (steps > 0) ? ctx->def->slider.up_usage : ctx->def->slider.down_usage;
    for (int i = 0; i < abs(steps); i++) {
        if (td_hid_send_consumer(usage) != ESP_OK) {
            toast("volume queue unavailable", td_theme_danger());
            break;
        }
    }
    ctx->last_slider_value = (int16_t)(ctx->last_slider_value + steps * step);
}

/* ------------------------------------------------------------------ widgets */

static lv_obj_t *make_card(lv_color_t accent, int32_t w, int32_t h, bool interactive)
{
    lv_obj_t *card = interactive ? lv_button_create(s_grid) : lv_obj_create(s_grid);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, w, h);
    td_theme_style_card(card, accent, interactive);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void position_card(lv_obj_t *card, const td_button_t *def, uint8_t index,
                          uint8_t cols, uint8_t rows)
{
    const uint8_t col = def->has_position ? def->col : index % cols;
    const uint8_t row = def->has_position ? def->row : index / cols;
    const uint8_t col_span = def->has_position ? def->col_span : 1;
    const uint8_t row_span = def->has_position ? def->row_span : 1;
    const int32_t content_w = TD_SCREEN_W - 2 * TD_GAP;
    const int32_t cell_w = (content_w - (cols - 1) * TD_GAP) / cols;
    const int32_t cell_h = (TD_CONTENT_H - (rows - 1) * TD_GAP) / rows;
    lv_obj_set_pos(card, col * (cell_w + TD_GAP), row * (cell_h + TD_GAP));
    lv_obj_set_size(card, col_span * cell_w + (col_span - 1) * TD_GAP,
                    row_span * cell_h + (row_span - 1) * TD_GAP);
}

static void build_tile(td_button_ctx_t *ctx, lv_obj_t *card)
{
    lv_obj_t *caption = lv_label_create(card);
    lv_label_set_text(caption, ctx->def->label);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(caption, td_theme_text_dim(), LV_PART_MAIN);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 4, 2);

    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_align(value, LV_ALIGN_LEFT_MID, 4, 4);
    ctx->value_label = value;

    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_set_size(bar, lv_pct(88), 8);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, td_theme_border(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, ctx->accent, LV_PART_INDICATOR);
    ctx->value_bar = bar;
}

static void build_slider(td_button_ctx_t *ctx, lv_obj_t *card, int32_t card_w)
{
    lv_obj_t *caption = lv_label_create(card);
    lv_label_set_text(caption, ctx->def->label);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *slider = lv_slider_create(card);
    lv_obj_set_width(slider, card_w - 56);
    lv_obj_set_height(slider, 14);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 14);

    lv_obj_set_style_bg_color(slider, td_theme_border(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, ctx->accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, ctx->accent, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(slider, 14, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, ctx->accent, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_50, LV_PART_KNOB);
    /* Knob is the only thing the finger has to hit, so give it real size. */
    lv_obj_set_style_pad_all(slider, 12, LV_PART_KNOB);

    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
}

static void nav_event_cb(lv_event_t *event)
{
    build_page((int)(intptr_t)lv_event_get_user_data(event));
}

static void build_nav(int active)
{
    lv_obj_clean(s_nav);
    const int visible = s_config->page_count > 4 ? 4 : s_config->page_count;
    for (int i = 0; i < visible; i++) {
        const td_page_t *page = &s_config->pages[i];
        lv_obj_t *item = lv_button_create(s_nav);
        lv_obj_remove_style_all(item);
        lv_obj_set_height(item, TD_NAV_H - 12);
        lv_obj_set_flex_grow(item, 1);
        lv_obj_set_style_radius(item, 12, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, page_accent(i), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item, i == active ? LV_OPA_20 : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(item, i == active ? page_accent(i) : td_theme_text_dim(),
                                    LV_PART_MAIN);
        lv_obj_add_event_cb(item, nav_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text_fmt(label, "%s  %s", icon_symbol(page->icon), page->title);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(label);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
}

/* ------------------------------------------------------------------ page */

static void build_page(int page_index)
{
    if (s_config == NULL || page_index < 0 || page_index >= s_config->page_count) {
        return;
    }
    s_page_index = page_index;
    const td_page_t *page = &s_config->pages[page_index];
    const lv_color_t accent = page_accent(page_index);

    lv_obj_clean(s_grid);
    memset(s_button_ctx, 0, sizeof(s_button_ctx));

    lv_label_set_text(s_page_label, page->title);
    lv_obj_set_style_text_color(s_page_label, accent, LV_PART_MAIN);
    build_nav(page_index);

    const uint8_t cols = page->cols != 0 ? page->cols : s_config->cols;
    const uint8_t rows = page->rows != 0 ? page->rows : s_config->rows;

    for (uint8_t i = 0; i < page->button_count && i < TD_CFG_MAX_BUTTONS; i++) {
        const td_button_t *def = &page->buttons[i];
        td_button_ctx_t *ctx = &s_button_ctx[i];
        ctx->def = def;
        ctx->accent = td_theme_accent_by_name(def->color, i);

        if (def->slider.enabled) {
            lv_obj_t *card = make_card(ctx->accent, 1, 1, false);
            position_card(card, def, i, cols, rows);
            ctx->last_slider_value = 50;
            build_slider(ctx, card, lv_obj_get_width(card));
            continue;
        }

        if (def->tile[0] != '\0') {
            lv_obj_t *card = make_card(ctx->accent, 1, 1, false);
            position_card(card, def, i, cols, rows);
            build_tile(ctx, card);
            continue;
        }

        lv_obj_t *card = make_card(ctx->accent, 1, 1, true);
        position_card(card, def, i, cols, rows);
        const int32_t card_w = lv_obj_get_width(card);
        const bool compact = strcmp(def->variant, "compact") == 0 ||
                             strcmp(def->variant, "utility") == 0;
        if (strcmp(def->variant, "utility") == 0) {
            lv_obj_set_style_bg_color(card, td_theme_surface_deep(), LV_PART_MAIN);
            lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
        }
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        const char *symbol = icon_symbol(def->icon);
        if (symbol[0] != '\0') {
            lv_obj_t *icon = lv_label_create(card);
            lv_label_set_text(icon, symbol);
            lv_obj_set_style_text_font(icon, compact ? &lv_font_montserrat_24 : &lv_font_montserrat_36,
                                       LV_PART_MAIN);
            lv_obj_align(icon, compact ? LV_ALIGN_LEFT_MID : LV_ALIGN_TOP_LEFT,
                         compact ? 8 : 6, compact ? 0 : 6);
            lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        }
        lv_obj_t *label = lv_label_create(card);
        lv_label_set_text(label, def->label);
        lv_obj_set_style_text_font(label, compact ? &lv_font_montserrat_20 : &lv_font_montserrat_28,
                                   LV_PART_MAIN);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, card_w - (compact && symbol[0] != '\0' ? 76 : 24));
        lv_obj_set_style_text_align(label, compact ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN);
        if (compact) {
            lv_obj_align(label, LV_ALIGN_LEFT_MID, symbol[0] != '\0' ? 58 : 8, -10);
        } else {
            lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, def->hint[0] != '\0' ? -28 : -12);
        }
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

        if (def->hint[0] != '\0') {
            lv_obj_t *hint = lv_label_create(card);
            lv_label_set_text(hint, def->hint);
            lv_obj_set_style_text_font(hint, &lv_font_montserrat_20, LV_PART_MAIN);
            lv_obj_set_style_text_color(hint, td_theme_text_dim(), LV_PART_MAIN);
            lv_obj_align(hint, compact ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_MID,
                         compact ? (symbol[0] != '\0' ? 58 : 8) : 0, -4);
            lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
        }

        /* A long-press alternative is invisible otherwise, so mark it. */
        if (def->has_long_press) {
            lv_obj_t *mark = lv_label_create(card);
            lv_label_set_text(mark, LV_SYMBOL_PLUS);
            lv_obj_set_style_text_font(mark, &lv_font_montserrat_20, LV_PART_MAIN);
            lv_obj_set_style_text_opa(mark, LV_OPA_50, LV_PART_MAIN);
            lv_obj_align(mark, LV_ALIGN_TOP_RIGHT, -2, 0);
            lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        }

        lv_obj_add_event_cb(card, button_event_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(card, button_event_cb, LV_EVENT_LONG_PRESSED, ctx);
    }

    ESP_LOGI(TAG, "page '%s': %u buttons", page->id, page->button_count);
}

/* ------------------------------------------------------------------ refresh */

static void refresh_tiles(void)
{
    const uint32_t packed = atomic_load(&s_telemetry_packed);
    const int64_t received_us = atomic_load(&s_telemetry_received_us);
    const bool fresh = (packed >> 24) != 0 && received_us != 0 &&
                       esp_timer_get_time() - received_us < 5 * 1000 * 1000;
    const uint8_t values[3] = {
        (uint8_t)(packed & 0xFF),
        (uint8_t)((packed >> 8) & 0xFF),
        (uint8_t)((packed >> 16) & 0xFF),
    };

    for (size_t i = 0; i < TD_CFG_MAX_BUTTONS; i++) {
        td_button_ctx_t *ctx = &s_button_ctx[i];
        if (ctx->value_label == NULL || ctx->def == NULL) {
            continue;
        }
        if (!fresh) {
            lv_label_set_text(ctx->value_label, "--");
            lv_obj_set_style_text_color(ctx->value_label, td_theme_text_dim(), LV_PART_MAIN);
            if (ctx->value_bar != NULL) {
                lv_bar_set_value(ctx->value_bar, 0, LV_ANIM_OFF);
            }
            continue;
        }
        int value = -1;
        if (strcmp(ctx->def->tile, "cpu") == 0) {
            value = values[0];
        } else if (strcmp(ctx->def->tile, "mem") == 0) {
            value = values[1];
        } else if (strcmp(ctx->def->tile, "disk") == 0) {
            value = values[2];
        }
        if (value < 0) {
            continue;
        }

        lv_label_set_text_fmt(ctx->value_label, "%d%%", value);
        /* Colour shifts to danger past 85%, but the number still states it. */
        const lv_color_t colour = (value > 85) ? td_theme_danger() : ctx->accent;
        lv_obj_set_style_text_color(ctx->value_label, colour, LV_PART_MAIN);
        if (ctx->value_bar != NULL) {
            lv_bar_set_value(ctx->value_bar, value, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(ctx->value_bar, colour, LV_PART_INDICATOR);
        }
    }
}

static void refresh_ack(void)
{
    static uint32_t last_counter;
    char detail[sizeof(s_ack_detail)];
    portENTER_CRITICAL(&s_state_mux);
    const uint32_t packed = atomic_load(&s_ack_packed);
    const uint32_t counter = packed >> 16;
    if (counter == last_counter) {
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }
    last_counter = counter;
    strlcpy(detail, s_ack_detail, sizeof(detail));
    portEXIT_CRITICAL(&s_state_mux);

    const uint8_t status = packed & 0xFF;
    char msg[64];
    snprintf(msg, sizeof(msg), "%s %s", status == TD_ACK_OK ? "ok" : "failed", detail);
    toast(msg, status == TD_ACK_OK ? td_theme_ok() : td_theme_danger());
}

/* Case insensitive "does the window title contain this pattern". */
static bool title_contains(const char *title, const char *pattern_lower)
{
    char lowered[sizeof(s_profile)];
    size_t i = 0;
    for (; title[i] != '\0' && i < sizeof(lowered) - 1; i++) {
        lowered[i] = (char)tolower((unsigned char)title[i]);
    }
    lowered[i] = '\0';
    return strstr(lowered, pattern_lower) != NULL;
}

static void refresh_profile(void)
{
    static uint32_t last_counter;
    char profile[sizeof(s_profile)];
    portENTER_CRITICAL(&s_state_mux);
    const uint32_t counter = atomic_load(&s_profile_counter);
    if (counter == last_counter || s_config == NULL) {
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }
    last_counter = counter;
    strlcpy(profile, s_profile, sizeof(profile));
    portEXIT_CRITICAL(&s_state_mux);

    for (uint8_t page = 0; page < s_config->page_count; page++) {
        for (uint8_t m = 0; m < s_config->pages[page].match_count; m++) {
            if (!title_contains(profile, s_config->pages[page].match[m])) {
                continue;
            }
            if (page != s_page_index) {
                ESP_LOGI(TAG, "profile '%s' -> page '%s'", profile, s_config->pages[page].id);
                build_page(page);
            }
            return;
        }
    }
}

static void refresh_gesture(void)
{
    static uint32_t last_counter;
    const uint32_t packed = atomic_load(&s_gesture_packed);
    const uint32_t counter = packed >> 8;
    if (counter == last_counter || s_config == NULL) {
        return;
    }
    last_counter = counter;

    switch ((td_gesture_t)(packed & 0xFF)) {
    case TD_GESTURE_NEXT_PAGE:
        build_page((s_page_index + 1) % s_config->page_count);
        break;
    case TD_GESTURE_PREV_PAGE:
        build_page((s_page_index + s_config->page_count - 1) % s_config->page_count);
        break;
    case TD_GESTURE_WAKE:
        lv_display_trigger_activity(NULL);
        if (s_dimmed) {
            s_dimmed = false;
            bsp_display_brightness_set(s_config->brightness);
        }
        break;
    case TD_GESTURE_NONE:
        break;
    }
}

/* Dims the backlight after inactivity; any touch restores it. */
static void refresh_screensaver(void)
{
    if (s_config == NULL || s_config->screensaver_sec == 0) {
        return;
    }
    const uint32_t idle_ms = lv_display_get_inactive_time(NULL);
    const bool should_dim = idle_ms > (uint32_t)s_config->screensaver_sec * 1000;

    if (should_dim == s_dimmed) {
        return;
    }
    s_dimmed = should_dim;
    bsp_display_brightness_set(should_dim ? 8 : s_config->brightness);
}

static void set_pill(lv_obj_t *pill, const char *text, lv_color_t colour)
{
    lv_obj_t *label = lv_obj_get_child(pill, 0);
    if (label == NULL) {
        return;
    }
    if (strcmp(lv_label_get_text(label), text) == 0) {
        return;
    }
    lv_label_set_text(label, text);
    td_theme_style_pill(pill, colour);
}

static void status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const bool usb = td_hid_is_connected();
    const bool agent = td_raw_hid_agent_present();

    /* Text states the status; colour only reinforces it. */
    set_pill(s_usb_pill, usb ? "USB linked" : "USB waiting",
             usb ? td_theme_ok() : td_theme_warn());
    set_pill(s_agent_pill, agent ? "agent online" : "agent offline",
             agent ? td_theme_ok() : td_theme_text_dim());

    refresh_tiles();
    refresh_ack();
    refresh_profile();
    refresh_gesture();
    refresh_screensaver();
}

/* ------------------------------------------------------------------ fallback deck */

static void add_fallback_button(td_page_t *page, const char *label, const char *k1, const char *k2)
{
    td_button_t *button = &page->buttons[page->button_count++];
    memset(button, 0, sizeof(*button));
    strlcpy(button->label, label, sizeof(button->label));
    button->action.type = TD_ACTION_HID_KEYS;

    const char *keys[2] = {k1, k2};
    for (int i = 0; i < 2; i++) {
        if (keys[i] == NULL) {
            continue;
        }
        uint8_t keycode = 0;
        if (td_keymap_lookup(keys[i], &keycode, &button->action.modifiers) && keycode != 0) {
            button->action.keys[button->action.key_count++] = keycode;
        }
    }
}

static const td_config_t *build_fallback_config(void)
{
    if (s_fallback == NULL) {
        s_fallback = td_config_alloc();
    }
    if (s_fallback == NULL) {
        return NULL;
    }

    memset(s_fallback, 0, sizeof(*s_fallback));
    s_fallback->cols = 3;
    s_fallback->rows = 2;
    s_fallback->platform = TD_PLATFORM_WINDOWS;
    s_fallback->brightness = 80;
    s_fallback->screensaver_sec = 120;
    s_fallback->page_count = 1;

    td_page_t *page = &s_fallback->pages[0];
    strlcpy(page->id, "fallback", sizeof(page->id));
    strlcpy(page->title, "Fallback deck", sizeof(page->title));

    add_fallback_button(page, "Copy", "PRIMARY", "C");
    add_fallback_button(page, "Paste", "PRIMARY", "V");
    add_fallback_button(page, "Undo", "PRIMARY", "Z");
    add_fallback_button(page, "Save", "PRIMARY", "S");
    add_fallback_button(page, "Switch", "ALT", "TAB");
    add_fallback_button(page, "Lock", "GUI", "L");

    return s_fallback;
}

/* ------------------------------------------------------------------ public API */

void td_ui_on_telemetry(const td_telemetry_t *telemetry)
{
    if (telemetry == NULL || !telemetry->valid) {
        return;
    }
    const uint32_t packed = (uint32_t)telemetry->cpu_percent |
                            ((uint32_t)telemetry->mem_percent << 8) |
                            ((uint32_t)telemetry->disk_percent << 16) | (1u << 24);
    atomic_store(&s_telemetry_packed, packed);
    atomic_store(&s_telemetry_received_us, esp_timer_get_time());
}

void td_ui_on_gesture(td_gesture_t gesture)
{
    const uint32_t counter = (atomic_load(&s_gesture_packed) >> 8) + 1;
    atomic_store(&s_gesture_packed, (uint32_t)gesture | (counter << 8));
}

void td_ui_on_profile(const char *profile)
{
    if (profile == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_mux);
    strlcpy(s_profile, profile, sizeof(s_profile));
    atomic_fetch_add(&s_profile_counter, 1);
    portEXIT_CRITICAL(&s_state_mux);
}

void td_ui_on_ack(uint8_t seq, uint8_t status, const char *detail)
{
    portENTER_CRITICAL(&s_state_mux);
    strlcpy(s_ack_detail, detail != NULL ? detail : "", sizeof(s_ack_detail));
    const uint32_t counter = (atomic_load(&s_ack_packed) >> 16) + 1;
    atomic_store(&s_ack_packed, (uint32_t)status | ((uint32_t)seq << 8) | (counter << 16));
    portEXIT_CRITICAL(&s_state_mux);
}

esp_err_t td_ui_reload_from_file(const char *config_path)
{
    char err[64] = {0};
    td_config_t *candidate = td_config_alloc();
    if (candidate == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = td_config_load(config_path, candidate, err, sizeof(err));
    if (result != ESP_OK) {
        char msg[80];
        snprintf(msg, sizeof(msg), "reload failed: %s", err);
        if (bsp_display_lock(1000)) {
            toast(msg, td_theme_danger());
            bsp_display_unlock();
        }
        td_config_free(candidate);
        return result;
    }

    if (!bsp_display_lock(1000)) {
        td_config_free(candidate);
        return ESP_ERR_TIMEOUT;
    }

    td_config_t *previous_reload = s_reloaded;
    s_reloaded = candidate;
    s_config = candidate;
    build_page(0);
    bsp_display_brightness_set(s_config->brightness);
    toast("config reloaded", td_theme_ok());
    bsp_display_unlock();

    /* build_page no longer keeps pointers into the previous uploaded model. */
    td_config_free(previous_reload);
    return ESP_OK;
}

static lv_obj_t *create_pill(lv_obj_t *parent)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_height(pill, 34);
    lv_obj_set_width(pill, LV_SIZE_CONTENT);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    td_theme_style_pill(pill, td_theme_text_dim());

    lv_obj_t *label = lv_label_create(pill);
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(label);
    return pill;
}

esp_err_t td_ui_start(const td_config_t *config, const char *load_error)
{
    s_config = (config != NULL) ? config : build_fallback_config();

    if (!bsp_display_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }

    lv_obj_t *screen = lv_screen_active();
    td_theme_apply_screen(screen);
    lv_obj_set_style_pad_all(screen, TD_GAP, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Header: page title on the left and explicit connectivity on the right. */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), TD_HEADER_H - TD_GAP);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_page_label = lv_label_create(header);
    lv_obj_set_style_text_font(s_page_label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_label_set_text(s_page_label, "");
    lv_obj_align(s_page_label, LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t *status_row = lv_obj_create(header);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_size(status_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(status_row, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 10, LV_PART_MAIN);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);

    s_usb_pill = create_pill(status_row);
    s_agent_pill = create_pill(status_row);

    /* One line of feedback remains visible immediately above navigation. */
    s_toast_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_toast_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_toast_label, td_theme_text_dim(), LV_PART_MAIN);
    lv_obj_align(s_toast_label, LV_ALIGN_BOTTOM_LEFT, 2, -(TD_NAV_H + 4));
    lv_label_set_text(s_toast_label, "");
    if (config == NULL && load_error != NULL) {
        toast(load_error, td_theme_danger());
    }

    s_grid = lv_obj_create(screen);
    lv_obj_remove_style_all(s_grid);
    lv_obj_set_size(s_grid, TD_SCREEN_W - 2 * TD_GAP, TD_CONTENT_H);
    lv_obj_align(s_grid, LV_ALIGN_TOP_MID, 0, TD_HEADER_H);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);

    s_nav = lv_obj_create(screen);
    lv_obj_remove_style_all(s_nav);
    lv_obj_set_size(s_nav, LV_PCT(100), TD_NAV_H);
    lv_obj_align(s_nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_nav, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_nav, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(s_nav, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_nav, td_theme_border(), LV_PART_MAIN);
    lv_obj_clear_flag(s_nav, LV_OBJ_FLAG_SCROLLABLE);

    build_page(0);
    bsp_display_brightness_set(s_config->brightness);
    lv_timer_create(status_timer_cb, 500, NULL);
    status_timer_cb(NULL);

    bsp_display_unlock();
    return ESP_OK;
}
