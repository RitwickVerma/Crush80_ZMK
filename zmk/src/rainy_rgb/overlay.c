#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/irq.h>
#include "overlay.h"
#include "color.h"
#include "led_map.h"     /* rrgb_led_for_position */

#define CAPS_POS         51   /* keymap position of CapsLock (home row) */
#define BAT_SHOW_FRAMES  150  /* ~3s at the engine's 50 fps (RRGB_FPS) */
#define BAT_SEG_FIRST    18   /* number row keys 1..0 = positions 18..27 */
#define BAT_SEG_COUNT    10

/* Colour of each keymap position while Fn is held (black = not lit), supplied
 * by the adapter from the live keymap and state, so it follows VIA and ZMK
 * Studio edits. */
static struct rrgb s_fn_px[RRGB_FN_POSITIONS];

static volatile bool     s_caps;
static volatile bool     s_fn;
static volatile uint32_t s_mode;   /* 0xRRGGBB, 0 = none */
static volatile uint8_t  s_battery;
static volatile uint32_t s_bat_until;

void rrgb_overlay_set_caps(bool on)        { s_caps = on; }
void rrgb_overlay_set_fn(bool active, const struct rrgb *fn_colors) {
    if (active && fn_colors) {
        unsigned key = irq_lock();   /* the render thread reads this table */
        memcpy(s_fn_px, fn_colors, sizeof(s_fn_px));
        irq_unlock(key);
    }
    s_fn = active;
}
void rrgb_overlay_set_mode(struct rrgb c) { s_mode = ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }
void rrgb_overlay_set_battery(uint8_t pct) { s_battery = pct > 100 ? 100 : pct; }
void rrgb_overlay_battery_show(uint32_t tick) { s_bat_until = tick + BAT_SHOW_FRAMES; }

bool rrgb_overlay_active(uint32_t tick) {
    return s_caps || s_fn || s_mode || (tick < s_bat_until);
}

static void set_pos(struct rrgb *px, uint16_t n, uint8_t pos, struct rrgb c) {
    uint8_t leds[RRGB_MAX_LEDS_PER_KEY];
    uint8_t cnt = rrgb_leds_for_position(pos, leds);
    for (uint8_t i = 0; i < cnt; i++) {
        if (leds[i] < n) { px[leds[i]] = c; }
    }
}

void rrgb_overlay_render(struct rrgb *px, uint16_t n, uint32_t tick) {
    /* 1. Fn-highlight: black out, light only Fn-active keys white. */
    if (s_fn) {
        for (uint16_t i = 0; i < n; i++) { px[i] = (struct rrgb){0, 0, 0}; }
        for (unsigned pos = 0; pos < RRGB_FN_POSITIONS; pos++) {
            struct rrgb c = s_fn_px[pos];

            if (c.r || c.g || c.b) {
                set_pos(px, n, (uint8_t)pos, c);
            }
        }
    }
    /* 2. CapsLock: white on the CapsLock key. */
    if (s_caps) {
        set_pos(px, n, CAPS_POS, (struct rrgb){255, 255, 255});
    }
    /* 2b. Editing mode: Esc and H J K L, the keys Vim mode is about. */
    if (s_mode && !s_fn) {
        struct rrgb c = {(uint8_t)(s_mode >> 16), (uint8_t)(s_mode >> 8), (uint8_t)s_mode};
        static const uint8_t mode_pos[] = {0, 57, 58, 59, 60}; /* Esc, H, J, K, L */

        for (unsigned i = 0; i < sizeof(mode_pos); i++) {
            set_pos(px, n, mode_pos[i], c);
        }
    }
    /* 3. Battery gauge: 10-segment bar on the number row, ~3s window. */
    if (tick < s_bat_until) {
        uint8_t lit = (uint8_t)((s_battery * BAT_SEG_COUNT + 50) / 100);  /* 0..10 */
        uint8_t hue = (uint8_t)(85 * (uint16_t)s_battery / 100);          /* 0%=red,100%=green */
        for (uint8_t s = 0; s < BAT_SEG_COUNT; s++) {
            struct rrgb c = (s < lit) ? hsv2rgb(hue, 255, 255)
                                      : (struct rrgb){8, 8, 8};
            set_pos(px, n, (uint8_t)(BAT_SEG_FIRST + s), c);
        }
    }
}
