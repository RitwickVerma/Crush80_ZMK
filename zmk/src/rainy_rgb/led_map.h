#ifndef RAINY_RGB_LED_MAP_H
#define RAINY_RGB_LED_MAP_H
#include <stdint.h>
#include "effects.h"

/* LED index -> normalized physical XY (uint8, uniform scale, origin top-left).
 * Points at the calibrated led_positions table (no longer NULL since Phase 2). */
extern const struct led_xy *rrgb_led_xy;

/* Physical XY of the key at the given keymap position — the origin for
 * keypress-driven spatial effects (ripple). Returns NULL if out of range. */
const struct led_xy *rrgb_xy_for_position(uint32_t position);

/* keymap position -> LED index, or -1 if out of range. */
int rrgb_led_for_position(uint32_t position);

/* All LEDs under the key at a keymap position (Caps Lock, Enter and Space have
 * more than one). Returns how many were written to out (0 if out of range). */
#define RRGB_MAX_LEDS_PER_KEY 3
uint8_t rrgb_leds_for_position(uint32_t position, uint8_t out[RRGB_MAX_LEDS_PER_KEY]);

/* Number of LEDs under keys (0..RRGB_KEY_LED_COUNT-1); the rest are side bar
 * and logo. */
#define RRGB_KEY_LED_COUNT 92

#endif
