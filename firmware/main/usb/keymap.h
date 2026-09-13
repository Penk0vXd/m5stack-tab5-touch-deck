/*
 * Key name -> HID usage code translation.
 *
 * Config files spell keys as text ("CTRL", "SHIFT", "F5", "ENTER", "A"), so the
 * same file works no matter how the firmware is built. "PRIMARY" is the
 * platform modifier: Ctrl on Windows and Linux, GUI (Cmd) on macOS.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TD_PLATFORM_WINDOWS = 0,
    TD_PLATFORM_LINUX,
    TD_PLATFORM_MACOS,
} td_platform_t;

/* Selects what "PRIMARY" maps to. Defaults to Windows. */
void td_keymap_set_platform(td_platform_t platform);
td_platform_t td_keymap_get_platform(void);

/*
 * Resolves one key name.
 * Modifier names set a bit in *modifier_mask; plain keys set *keycode.
 * Returns false for unknown names so the config loader can report them.
 */
bool td_keymap_lookup(const char *name, uint8_t *keycode, uint8_t *modifier_mask);

/* Consumer control usage for media key names ("VOLUME_UP", "PLAY_PAUSE", ...). */
bool td_keymap_lookup_consumer(const char *name, uint16_t *usage);

#ifdef __cplusplus
}
#endif
