/*
 * Touch Deck visual theme: "neon HUD on OLED black".
 *
 * One dark canvas, one accent hue per button. Colour carries meaning here -
 * it groups actions by category - so every accent is also backed by its label
 * text, never by colour alone.
 *
 * All accents are checked against the surface colour for at least 4.5:1
 * contrast as text, and the pressed state flips to a dark-on-accent fill so
 * the contrast holds in both states.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Canvas and surfaces */
lv_color_t td_theme_bg(void);
lv_color_t td_theme_surface(void);
lv_color_t td_theme_surface_deep(void);
lv_color_t td_theme_border(void);

/* Text */
lv_color_t td_theme_text(void);
lv_color_t td_theme_text_dim(void);
lv_color_t td_theme_text_on_accent(void);

/* Semantic status colours */
lv_color_t td_theme_ok(void);
lv_color_t td_theme_warn(void);
lv_color_t td_theme_danger(void);

/* Accent ramp used to colour-code buttons */
#define TD_ACCENT_COUNT 6
lv_color_t td_theme_accent(uint8_t index);

/*
 * Resolves a config colour: a name ("cyan", "green", "violet", "amber",
 * "rose", "blue") or a "#RRGGBB" literal. Empty or unknown values fall back
 * to the accent ramp at fallback_index, so a config with no colours at all
 * still comes out colour-coded.
 */
lv_color_t td_theme_accent_by_name(const char *name, uint8_t fallback_index);

/* Paints the root screen. */
void td_theme_apply_screen(lv_obj_t *screen);

/*
 * Card surface with an accent border and glow. When interactive, adds the
 * pressed state (accent fill, dark text, slight shrink) and a 150 ms
 * transition so the press reads as motion rather than a snap.
 */
void td_theme_style_card(lv_obj_t *obj, lv_color_t accent, bool interactive);

/* Compact status badge: accent-tinted fill, accent border, accent text. */
void td_theme_style_pill(lv_obj_t *obj, lv_color_t accent);

#ifdef __cplusplus
}
#endif
