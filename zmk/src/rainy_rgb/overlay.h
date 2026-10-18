#ifndef RAINY_RGB_OVERLAY_H
#define RAINY_RGB_OVERLAY_H
#include <stdint.h>
#include <stdbool.h>
#include "color.h"   /* struct rrgb */

/* Neutral state — set by the adapter from the ZMK event thread. */
void rrgb_overlay_set_caps(bool on);
/* Fn held: light every keymap position whose colour in fn_colors is not black
 * (RRGB_FN_POSITIONS entries, copied; ignored when inactive). The adapter sets
 * white for a bound key and a state colour for keys that show state. */
#define RRGB_FN_POSITIONS 88
void rrgb_overlay_set_fn(bool active, const struct rrgb *fn_colors);
/* Editing mode (Vim / Visual / operator pending): Esc and H J K L glow in this colour
 * while it is not black. Set from the layer state, so the mode is always visible. */
void rrgb_overlay_set_mode(struct rrgb color);
void rrgb_overlay_set_battery(uint8_t pct);
void rrgb_overlay_battery_show(uint32_t tick);   /* start the ~3s gauge window */

/* Applied AFTER the effect, BEFORE strip_show. */
void rrgb_overlay_render(struct rrgb *px, uint16_t n, uint32_t tick);

/* True if any functional overlay needs to show this frame (caps on, Fn held,
 * or battery gauge window open) — lets the engine render indicators even when
 * the decorative RGB is toggled off. */
bool rrgb_overlay_active(uint32_t tick);

#endif
