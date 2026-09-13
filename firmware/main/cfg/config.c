#include "cfg/config.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "td_cfg";

#define MAX_CONFIG_BYTES (64 * 1024)

td_config_t *td_config_alloc(void)
{
    td_config_t *config = heap_caps_calloc(1, sizeof(td_config_t), MALLOC_CAP_SPIRAM);
    if (config == NULL) {
        ESP_LOGW(TAG, "PSRAM allocation failed, falling back to internal RAM");
        config = calloc(1, sizeof(td_config_t));
    }
    return config;
}

void td_config_free(td_config_t *config)
{
    free(config);
}

static void set_err(char *err_msg, size_t len, const char *fmt, ...)
{
    if (err_msg == NULL || len == 0) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(err_msg, len, fmt, args);
    va_end(args);
}

static char *read_file(const char *path, char *err_msg, size_t err_len)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        set_err(err_msg, err_len, "cannot open %s", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > MAX_CONFIG_BYTES) {
        set_err(err_msg, err_len, "config size %ld out of range", size);
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        set_err(err_msg, err_len, "out of memory (%ld bytes)", size);
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[read] = '\0';
    return buffer;
}

static void copy_string(char *dst, size_t dst_len, const cJSON *node, const char *fallback)
{
    const char *value = cJSON_IsString(node) ? node->valuestring : fallback;
    if (value == NULL) {
        dst[0] = '\0';
        return;
    }
    strlcpy(dst, value, dst_len);
}

static bool parse_action(const cJSON *node, td_action_t *action, td_macro_t *macro,
                         char *err_msg, size_t err_len);

static bool parse_keys(const cJSON *keys, td_action_t *action, char *err_msg, size_t err_len)
{
    const cJSON *key = NULL;
    cJSON_ArrayForEach(key, keys) {
        if (!cJSON_IsString(key)) {
            set_err(err_msg, err_len, "keys must be strings");
            return false;
        }
        uint8_t keycode = 0;
        if (!td_keymap_lookup(key->valuestring, &keycode, &action->modifiers)) {
            set_err(err_msg, err_len, "unknown key '%s'", key->valuestring);
            return false;
        }
        if (keycode != 0) {
            if (action->key_count >= TD_CFG_MAX_KEYS) {
                set_err(err_msg, err_len, "too many keys in one chord");
                return false;
            }
            action->keys[action->key_count++] = keycode;
        }
    }
    return true;
}

static bool parse_macro(const cJSON *steps, td_macro_t *macro, char *err_msg, size_t err_len)
{
    const cJSON *step = NULL;
    cJSON_ArrayForEach(step, steps) {
        if (macro->step_count >= TD_CFG_MAX_MACRO_STEPS) {
            set_err(err_msg, err_len, "macro longer than %d steps", TD_CFG_MAX_MACRO_STEPS);
            return false;
        }
        if (!parse_action(step, &macro->steps[macro->step_count], NULL, err_msg, err_len)) {
            return false;
        }
        macro->step_count++;
    }
    return true;
}

static bool parse_action(const cJSON *node, td_action_t *action, td_macro_t *macro,
                         char *err_msg, size_t err_len)
{
    memset(action, 0, sizeof(*action));
    if (!cJSON_IsObject(node)) {
        action->type = TD_ACTION_NONE;
        return true;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(node, "type");
    if (!cJSON_IsString(type)) {
        set_err(err_msg, err_len, "action without type");
        return false;
    }

    if (strcmp(type->valuestring, "hid_keys") == 0) {
        action->type = TD_ACTION_HID_KEYS;
        return parse_keys(cJSON_GetObjectItemCaseSensitive(node, "keys"), action, err_msg, err_len);
    }
    if (strcmp(type->valuestring, "hid_consumer") == 0) {
        action->type = TD_ACTION_HID_CONSUMER;
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(node, "key");
        if (!cJSON_IsString(key) ||
            !td_keymap_lookup_consumer(key->valuestring, &action->consumer_usage)) {
            set_err(err_msg, err_len, "unknown consumer key");
            return false;
        }
        return true;
    }
    if (strcmp(type->valuestring, "text") == 0) {
        action->type = TD_ACTION_TEXT;
        copy_string(action->text, sizeof(action->text),
                    cJSON_GetObjectItemCaseSensitive(node, "text"), "");
        return true;
    }
    if (strcmp(type->valuestring, "agent") == 0) {
        action->type = TD_ACTION_AGENT;
        copy_string(action->command, sizeof(action->command),
                    cJSON_GetObjectItemCaseSensitive(node, "command"), "");
        if (action->command[0] == '\0') {
            set_err(err_msg, err_len, "agent action without command");
            return false;
        }
        return true;
    }
    if (strcmp(type->valuestring, "goto_page") == 0) {
        action->type = TD_ACTION_GOTO_PAGE;
        copy_string(action->page, sizeof(action->page),
                    cJSON_GetObjectItemCaseSensitive(node, "page"), "");
        return true;
    }
    if (strcmp(type->valuestring, "delay") == 0) {
        action->type = TD_ACTION_DELAY;
        const cJSON *ms = cJSON_GetObjectItemCaseSensitive(node, "ms");
        action->delay_ms = cJSON_IsNumber(ms) ? (uint32_t)ms->valuedouble : 100;
        return true;
    }
    if (strcmp(type->valuestring, "brightness") == 0) {
        const cJSON *percent = cJSON_GetObjectItemCaseSensitive(node, "percent");
        if (!cJSON_IsNumber(percent) || percent->valueint < 1 || percent->valueint > 100) {
            set_err(err_msg, err_len, "brightness percent must be 1..100");
            return false;
        }
        action->type = TD_ACTION_BRIGHTNESS;
        action->brightness = (uint8_t)percent->valueint;
        return true;
    }
    if (strcmp(type->valuestring, "macro") == 0) {
        if (macro == NULL) {
            set_err(err_msg, err_len, "nested macros are not supported");
            return false;
        }
        action->type = TD_ACTION_MACRO;
        memset(macro, 0, sizeof(*macro));
        return parse_macro(cJSON_GetObjectItemCaseSensitive(node, "steps"), macro,
                           err_msg, err_len);
    }

    set_err(err_msg, err_len, "unknown action type '%s'", type->valuestring);
    return false;
}

static bool parse_button(const cJSON *node, td_button_t *button, char *err_msg, size_t err_len)
{
    memset(button, 0, sizeof(*button));
    copy_string(button->label, sizeof(button->label),
                cJSON_GetObjectItemCaseSensitive(node, "label"), "?");
    copy_string(button->icon, sizeof(button->icon),
                cJSON_GetObjectItemCaseSensitive(node, "icon"), "");
    copy_string(button->hint, sizeof(button->hint),
                cJSON_GetObjectItemCaseSensitive(node, "hint"), "");
    copy_string(button->variant, sizeof(button->variant),
                cJSON_GetObjectItemCaseSensitive(node, "variant"), "");
    copy_string(button->tile, sizeof(button->tile),
                cJSON_GetObjectItemCaseSensitive(node, "tile"), "");
    copy_string(button->color, sizeof(button->color),
                cJSON_GetObjectItemCaseSensitive(node, "color"), "");

    const cJSON *slider = cJSON_GetObjectItemCaseSensitive(node, "slider");
    if (cJSON_IsObject(slider)) {
        const cJSON *up = cJSON_GetObjectItemCaseSensitive(slider, "key_up");
        const cJSON *down = cJSON_GetObjectItemCaseSensitive(slider, "key_down");
        const cJSON *step = cJSON_GetObjectItemCaseSensitive(slider, "step");
        if (!cJSON_IsString(up) || !cJSON_IsString(down) ||
            !td_keymap_lookup_consumer(up->valuestring, &button->slider.up_usage) ||
            !td_keymap_lookup_consumer(down->valuestring, &button->slider.down_usage)) {
            set_err(err_msg, err_len, "slider '%s' needs valid key_up and key_down", button->label);
            return false;
        }
        if (step == NULL) {
            button->slider.step = 4;
        } else if (!cJSON_IsNumber(step) || step->valueint < 1 || step->valueint > 100) {
            set_err(err_msg, err_len, "slider '%s' step must be 1..100", button->label);
            return false;
        } else {
            button->slider.step = (uint8_t)step->valueint;
        }
        button->slider.enabled = true;
    }

    if (!parse_action(cJSON_GetObjectItemCaseSensitive(node, "action"), &button->action,
                      &button->macro, err_msg, err_len)) {
        return false;
    }

    const cJSON *long_press = cJSON_GetObjectItemCaseSensitive(node, "long_press");
    if (cJSON_IsObject(long_press)) {
        if (!parse_action(long_press, &button->long_press, &button->long_press_macro,
                          err_msg, err_len)) {
            return false;
        }
        button->has_long_press = true;
    }

    const cJSON *position = cJSON_GetObjectItemCaseSensitive(node, "position");
    if (cJSON_IsObject(position)) {
        const cJSON *col = cJSON_GetObjectItemCaseSensitive(position, "col");
        const cJSON *row = cJSON_GetObjectItemCaseSensitive(position, "row");
        const cJSON *col_span = cJSON_GetObjectItemCaseSensitive(position, "col_span");
        const cJSON *row_span = cJSON_GetObjectItemCaseSensitive(position, "row_span");
        if (!cJSON_IsNumber(col) || !cJSON_IsNumber(row) || col->valueint < 0 ||
            row->valueint < 0 || col->valueint > 11 || row->valueint > 5) {
            set_err(err_msg, err_len, "button '%s' has invalid position", button->label);
            return false;
        }
        const int cs = cJSON_IsNumber(col_span) ? col_span->valueint : 1;
        const int rs = cJSON_IsNumber(row_span) ? row_span->valueint : 1;
        if (cs < 1 || cs > 12 || rs < 1 || rs > 6) {
            set_err(err_msg, err_len, "button '%s' has invalid span", button->label);
            return false;
        }
        button->has_position = true;
        button->col = (uint8_t)col->valueint;
        button->row = (uint8_t)row->valueint;
        button->col_span = (uint8_t)cs;
        button->row_span = (uint8_t)rs;
    }
    return true;
}

static bool parse_page(const cJSON *node, td_page_t *page, uint8_t default_cols,
                       uint8_t default_rows, char *err_msg, size_t err_len)
{
    memset(page, 0, sizeof(*page));
    copy_string(page->id, sizeof(page->id), cJSON_GetObjectItemCaseSensitive(node, "id"), "page");
    copy_string(page->title, sizeof(page->title),
                cJSON_GetObjectItemCaseSensitive(node, "title"), page->id);
    copy_string(page->icon, sizeof(page->icon),
                cJSON_GetObjectItemCaseSensitive(node, "icon"), "");
    page->cols = default_cols;
    page->rows = default_rows;

    const cJSON *grid = cJSON_GetObjectItemCaseSensitive(node, "grid");
    if (cJSON_IsObject(grid)) {
        const cJSON *cols = cJSON_GetObjectItemCaseSensitive(grid, "cols");
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(grid, "rows");
        if (cJSON_IsNumber(cols) && cols->valueint >= 1 && cols->valueint <= 12) {
            page->cols = (uint8_t)cols->valueint;
        }
        if (cJSON_IsNumber(rows) && rows->valueint >= 1 && rows->valueint <= 6) {
            page->rows = (uint8_t)rows->valueint;
        }
    }

    const cJSON *match = cJSON_GetObjectItemCaseSensitive(node, "match");
    const cJSON *pattern = NULL;
    cJSON_ArrayForEach(pattern, match) {
        if (!cJSON_IsString(pattern) || page->match_count >= TD_CFG_MAX_MATCH) {
            continue;
        }
        char *dst = page->match[page->match_count++];
        strlcpy(dst, pattern->valuestring, TD_CFG_MATCH_LEN);
        for (char *c = dst; *c != '\0'; c++) {
            *c = (char)tolower((unsigned char)*c);
        }
    }

    const cJSON *buttons = cJSON_GetObjectItemCaseSensitive(node, "buttons");
    const cJSON *button = NULL;
    bool occupied[6][12] = {0};
    cJSON_ArrayForEach(button, buttons) {
        if (page->button_count >= TD_CFG_MAX_BUTTONS) {
            set_err(err_msg, err_len, "page '%s' has more than %d buttons", page->id,
                    TD_CFG_MAX_BUTTONS);
            return false;
        }
        if (!parse_button(button, &page->buttons[page->button_count], err_msg, err_len)) {
            return false;
        }
        td_button_t *parsed = &page->buttons[page->button_count];
        const uint8_t col = parsed->has_position ? parsed->col : page->button_count % page->cols;
        const uint8_t row = parsed->has_position ? parsed->row : page->button_count / page->cols;
        const uint8_t col_span = parsed->has_position ? parsed->col_span : 1;
        const uint8_t row_span = parsed->has_position ? parsed->row_span : 1;
        if (col + col_span > page->cols || row + row_span > page->rows) {
            set_err(err_msg, err_len, "button '%s' exceeds page grid", parsed->label);
            return false;
        }
        for (uint8_t y = row; y < row + row_span; y++) {
            for (uint8_t x = col; x < col + col_span; x++) {
                if (occupied[y][x]) {
                    set_err(err_msg, err_len, "button '%s' overlaps another button", parsed->label);
                    return false;
                }
                occupied[y][x] = true;
            }
        }
        page->button_count++;
    }
    return true;
}

esp_err_t td_config_load(const char *config_path, td_config_t *out_config,
                         char *err_msg, size_t err_msg_len)
{
    if (config_path == NULL || out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *raw = read_file(config_path, err_msg, err_msg_len);
    if (raw == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *root = cJSON_Parse(raw);
    if (root == NULL) {
        const char *pos = cJSON_GetErrorPtr();
        set_err(err_msg, err_msg_len, "JSON syntax error near '%.16s'", pos ? pos : "?");
        free(raw);
        return ESP_ERR_INVALID_ARG;
    }
    free(raw);

    memset(out_config, 0, sizeof(*out_config));
    out_config->cols = 5;
    out_config->rows = 3;
    out_config->platform = TD_PLATFORM_WINDOWS;
    out_config->brightness = 80;
    out_config->screensaver_sec = 120;

    const cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (cJSON_IsNumber(brightness) && brightness->valueint >= 10 && brightness->valueint <= 100) {
        out_config->brightness = (uint8_t)brightness->valueint;
    }
    const cJSON *screensaver = cJSON_GetObjectItemCaseSensitive(root, "screensaver_sec");
    if (cJSON_IsNumber(screensaver) && screensaver->valueint >= 0) {
        out_config->screensaver_sec = (uint16_t)screensaver->valueint;
    }

    const cJSON *platform = cJSON_GetObjectItemCaseSensitive(root, "platform");
    if (cJSON_IsString(platform)) {
        if (strcmp(platform->valuestring, "macos") == 0) {
            out_config->platform = TD_PLATFORM_MACOS;
        } else if (strcmp(platform->valuestring, "linux") == 0) {
            out_config->platform = TD_PLATFORM_LINUX;
        }
    }
    /* Key names resolve against the platform, so set it before parsing pages. */
    td_keymap_set_platform(out_config->platform);

    const cJSON *grid = cJSON_GetObjectItemCaseSensitive(root, "grid");
    if (cJSON_IsObject(grid)) {
        const cJSON *cols = cJSON_GetObjectItemCaseSensitive(grid, "cols");
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(grid, "rows");
        if (cJSON_IsNumber(cols) && cols->valueint > 0 && cols->valueint <= 12) {
            out_config->cols = (uint8_t)cols->valueint;
        }
        if (cJSON_IsNumber(rows) && rows->valueint > 0 && rows->valueint <= 6) {
            out_config->rows = (uint8_t)rows->valueint;
        }
    }

    const cJSON *pages = cJSON_GetObjectItemCaseSensitive(root, "pages");
    if (!cJSON_IsArray(pages) || cJSON_GetArraySize(pages) == 0) {
        set_err(err_msg, err_msg_len, "config has no pages");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *page = NULL;
    cJSON_ArrayForEach(page, pages) {
        if (out_config->page_count >= TD_CFG_MAX_PAGES) {
            set_err(err_msg, err_msg_len, "more than %d pages", TD_CFG_MAX_PAGES);
            cJSON_Delete(root);
            return ESP_ERR_INVALID_SIZE;
        }
        if (!parse_page(page, &out_config->pages[out_config->page_count],
                        out_config->cols, out_config->rows, err_msg, err_msg_len)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        out_config->page_count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "loaded %d pages from %s", out_config->page_count, config_path);
    return ESP_OK;
}

int td_config_find_page(const td_config_t *config, const char *page_id)
{
    if (config == NULL || page_id == NULL) {
        return -1;
    }
    for (uint8_t i = 0; i < config->page_count; i++) {
        if (strcmp(config->pages[i].id, page_id) == 0) {
            return i;
        }
    }
    return -1;
}
