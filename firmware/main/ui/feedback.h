/*
 * Audible touch feedback through the built-in speaker.
 *
 * A short click makes the deck usable without looking at it. Failure to open
 * the codec is not fatal: the click simply becomes a no-op.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t td_feedback_init(void);

/* Non-blocking: hands a pre-rendered click to the audio task. */
void td_feedback_click(void);

/* Lower pitched blip used for errors and refused actions. */
void td_feedback_error(void);

bool td_feedback_available(void);

#ifdef __cplusplus
}
#endif
