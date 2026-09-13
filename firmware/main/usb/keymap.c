#include "usb/keymap.h"

#include <stdlib.h>
#include <string.h>

#include "class/hid/hid.h"

static td_platform_t s_platform = TD_PLATFORM_WINDOWS;

typedef struct {
    const char *name;
    uint8_t keycode;
} td_key_entry_t;

/* Named keys. Single letters and digits are handled arithmetically below. */
static const td_key_entry_t k_named_keys[] = {
    {"ENTER", HID_KEY_ENTER},         {"RETURN", HID_KEY_ENTER},
    {"ESC", HID_KEY_ESCAPE},          {"ESCAPE", HID_KEY_ESCAPE},
    {"BACKSPACE", HID_KEY_BACKSPACE}, {"TAB", HID_KEY_TAB},
    {"SPACE", HID_KEY_SPACE},         {"MINUS", HID_KEY_MINUS},
    {"EQUAL", HID_KEY_EQUAL},         {"LEFTBRACE", HID_KEY_BRACKET_LEFT},
    {"RIGHTBRACE", HID_KEY_BRACKET_RIGHT}, {"BACKSLASH", HID_KEY_BACKSLASH},
    {"SEMICOLON", HID_KEY_SEMICOLON}, {"APOSTROPHE", HID_KEY_APOSTROPHE},
    {"GRAVE", HID_KEY_GRAVE},         {"COMMA", HID_KEY_COMMA},
    {"DOT", HID_KEY_PERIOD},          {"PERIOD", HID_KEY_PERIOD},
    {"SLASH", HID_KEY_SLASH},         {"CAPSLOCK", HID_KEY_CAPS_LOCK},
    {"PRINTSCREEN", HID_KEY_PRINT_SCREEN},
    {"INSERT", HID_KEY_INSERT},       {"HOME", HID_KEY_HOME},
    {"PAGEUP", HID_KEY_PAGE_UP},      {"DELETE", HID_KEY_DELETE},
    {"END", HID_KEY_END},             {"PAGEDOWN", HID_KEY_PAGE_DOWN},
    {"RIGHT", HID_KEY_ARROW_RIGHT},   {"LEFT", HID_KEY_ARROW_LEFT},
    {"DOWN", HID_KEY_ARROW_DOWN},     {"UP", HID_KEY_ARROW_UP},
};

typedef struct {
    const char *name;
    uint16_t usage;
} td_consumer_entry_t;

static const td_consumer_entry_t k_consumer_keys[] = {
    {"VOLUME_UP", HID_USAGE_CONSUMER_VOLUME_INCREMENT},
    {"VOLUME_DOWN", HID_USAGE_CONSUMER_VOLUME_DECREMENT},
    {"MUTE", HID_USAGE_CONSUMER_MUTE},
    {"PLAY_PAUSE", HID_USAGE_CONSUMER_PLAY_PAUSE},
    {"NEXT_TRACK", HID_USAGE_CONSUMER_SCAN_NEXT_TRACK},
    {"PREV_TRACK", HID_USAGE_CONSUMER_SCAN_PREVIOUS_TRACK},
    {"STOP", HID_USAGE_CONSUMER_STOP},
    {"BRIGHTNESS_UP", HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT},
    {"BRIGHTNESS_DOWN", HID_USAGE_CONSUMER_BRIGHTNESS_DECREMENT},
};

void td_keymap_set_platform(td_platform_t platform)
{
    s_platform = platform;
}

td_platform_t td_keymap_get_platform(void)
{
    return s_platform;
}

static uint8_t primary_modifier(void)
{
    return (s_platform == TD_PLATFORM_MACOS) ? KEYBOARD_MODIFIER_LEFTGUI
                                             : KEYBOARD_MODIFIER_LEFTCTRL;
}

bool td_keymap_lookup(const char *name, uint8_t *keycode, uint8_t *modifier_mask)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    /* Modifiers first: they add to the mask instead of occupying a key slot. */
    if (strcmp(name, "PRIMARY") == 0) {
        *modifier_mask |= primary_modifier();
        return true;
    }
    if (strcmp(name, "CTRL") == 0 || strcmp(name, "CONTROL") == 0) {
        *modifier_mask |= KEYBOARD_MODIFIER_LEFTCTRL;
        return true;
    }
    if (strcmp(name, "SHIFT") == 0) {
        *modifier_mask |= KEYBOARD_MODIFIER_LEFTSHIFT;
        return true;
    }
    if (strcmp(name, "ALT") == 0 || strcmp(name, "OPTION") == 0) {
        *modifier_mask |= KEYBOARD_MODIFIER_LEFTALT;
        return true;
    }
    if (strcmp(name, "GUI") == 0 || strcmp(name, "WIN") == 0 || strcmp(name, "CMD") == 0) {
        *modifier_mask |= KEYBOARD_MODIFIER_LEFTGUI;
        return true;
    }

    /* Single character: letter or digit. */
    if (name[1] == '\0') {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') {
            *keycode = HID_KEY_A + (c - 'A');
            return true;
        }
        if (c >= 'a' && c <= 'z') {
            *keycode = HID_KEY_A + (c - 'a');
            return true;
        }
        if (c == '0') {
            *keycode = HID_KEY_0;
            return true;
        }
        if (c >= '1' && c <= '9') {
            *keycode = HID_KEY_1 + (c - '1');
            return true;
        }
        return false;
    }

    /* Function keys F1..F24 */
    if (name[0] == 'F' && name[1] >= '0' && name[1] <= '9') {
        int n = atoi(name + 1);
        if (n >= 1 && n <= 12) {
            *keycode = HID_KEY_F1 + (n - 1);
            return true;
        }
        if (n >= 13 && n <= 24) {
            *keycode = HID_KEY_F13 + (n - 13);
            return true;
        }
        return false;
    }

    for (size_t i = 0; i < sizeof(k_named_keys) / sizeof(k_named_keys[0]); i++) {
        if (strcmp(name, k_named_keys[i].name) == 0) {
            *keycode = k_named_keys[i].keycode;
            return true;
        }
    }

    return false;
}

bool td_keymap_lookup_consumer(const char *name, uint16_t *usage)
{
    if (name == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof(k_consumer_keys) / sizeof(k_consumer_keys[0]); i++) {
        if (strcmp(name, k_consumer_keys[i].name) == 0) {
            *usage = k_consumer_keys[i].usage;
            return true;
        }
    }
    return false;
}
