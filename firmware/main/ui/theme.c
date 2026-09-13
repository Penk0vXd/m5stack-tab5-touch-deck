#include "ui/theme.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ palette */

#define TD_COL_BG            0x070A12
#define TD_COL_SURFACE       0x151E30
#define TD_COL_SURFACE_DEEP  0x0B1220
#define TD_COL_BORDER        0x243044
#define TD_COL_TEXT          0xF1F5F9
#define TD_COL_TEXT_DIM      0x94A3B8
#define TD_COL_ON_ACCENT     0x05070D

#define TD_COL_OK            0x22C55E
#define TD_COL_WARN          0xF59E0B
#define TD_COL_DANGER        0xF43F5E

static const uint32_t k_accents[TD_ACCENT_COUNT] = {
    0x22D3EE,  /* cyan   */
    0x22C55E,  /* green  */
    0xA78BFA,  /* violet */
    0xF59E0B,  /* amber  */
    0xF43F5E,  /* rose   */
    0x3B82F6,  /* blue   */
};

static const struct {
    const char *name;
    uint32_t value;
} k_named_accents[] = {
    {"cyan", 0x22D3EE},   {"green", 0x22C55E},  {"violet", 0xA78BFA},
    {"purple", 0xA78BFA}, {"amber", 0xF59E0B},  {"orange", 0xF59E0B},
    {"rose", 0xF43F5E},   {"red", 0xF43F5E},    {"blue", 0x3B82F6},
};

lv_color_t td_theme_bg(void) { return lv_color_hex(TD_COL_BG); }
lv_color_t td_theme_surface(void) { return lv_color_hex(TD_COL_SURFACE); }
lv_color_t td_theme_surface_deep(void) { return lv_color_hex(TD_COL_SURFACE_DEEP); }
lv_color_t td_theme_border(void) { return lv_color_hex(TD_COL_BORDER); }
lv_color_t td_theme_text(void) { return lv_color_hex(TD_COL_TEXT); }
lv_color_t td_theme_text_dim(void) { return lv_color_hex(TD_COL_TEXT_DIM); }
lv_color_t td_theme_text_on_accent(void) { return lv_color_hex(TD_COL_ON_ACCENT); }
lv_color_t td_theme_ok(void) { return lv_color_hex(TD_COL_OK); }
lv_color_t td_theme_warn(void) { return lv_color_hex(TD_COL_WARN); }
lv_color_t td_theme_danger(void) { return lv_color_hex(TD_COL_DANGER); }

lv_color_t td_theme_accent(uint8_t index)
{
    return lv_color_hex(k_accents[index % TD_ACCENT_COUNT]);
}

lv_color_t td_theme_accent_by_name(const char *name, uint8_t fallback_index)
{
    if (name == NULL || name[0] == '\0') {
        return td_theme_accent(fallback_index);
    }

    if (name[0] == '#') {
        const uint32_t value = (uint32_t)strtoul(name + 1, NULL, 16);
        return lv_color_hex(value);
    }

    for (size_t i = 0; i < sizeof(k_named_accents) / sizeof(k_named_accents[0]); i++) {
        if (strcmp(name, k_named_accents[i].name) == 0) {
            return lv_color_hex(k_named_accents[i].value);
        }
    }
    return td_theme_accent(fallback_index);
}

/* ------------------------------------------------------------------ styling */

void td_theme_apply_screen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, td_theme_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen, td_theme_text(), LV_PART_MAIN);
}

void td_theme_style_card(lv_obj_t *obj, lv_color_t accent, bool interactive)
{
    /* Keep redraws local: gradients, shadows and scale transitions flicker on DSI. */
    lv_obj_set_style_radius(obj, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, td_theme_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, accent, LV_PART_MAIN);
    lv_obj_set_style_border_opa(obj, LV_OPA_50, LV_PART_MAIN);

    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);

    /* Labels inherit this, so the accent carries the text too. */
    lv_obj_set_style_text_color(obj, accent, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 10, LV_PART_MAIN);

    if (!interactive) {
        return;
    }

    /* Pressed feedback is immediate and does not animate the whole card. */
    lv_obj_set_style_bg_color(obj, accent, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(obj, td_theme_text_on_accent(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
}

void td_theme_style_pill(lv_obj_t *obj, lv_color_t accent)
{
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, accent, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, accent, LV_PART_MAIN);
    lv_obj_set_style_border_opa(obj, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, accent, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(obj, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(obj, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
}
