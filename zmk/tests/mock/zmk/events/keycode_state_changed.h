#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Test side records the events. */
int raise_zmk_keycode_state_changed_from_encoded(uint32_t encoded, bool pressed, int64_t timestamp);
