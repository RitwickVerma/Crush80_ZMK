/*
 * Translation between VIA's QMK keycodes (protocol 12 numbering) and ZMK
 * bindings. Pure functions with no ZMK dependencies, so they are unit tested
 * on the host; via.c maps ZMK behavior devices to enum vk_beh.
 */

#ifndef VIA_XLAT_H
#define VIA_XLAT_H

#include <stdbool.h>
#include <stdint.h>

enum vk_beh {
    VK_NONE,   /* &none */
    VK_TRANS,  /* &trans */
    VK_KP,     /* &kp: p1 = ZMK keycode (usage page << 16 | id, modifiers << 24) */
    VK_MO,     /* &mo: p1 = layer */
    VK_TOG,    /* &tog: p1 = layer */
    VK_TO,     /* &to: p1 = layer */
    VK_LT,     /* &lt: p1 = layer, p2 = ZMK keycode */
    VK_BT,     /* &bt */
    VK_OUT,    /* &out */
    VK_RGB,    /* &rgb */
    VK_STUDIO, /* &studio_unlock */
    VK_OTHER,  /* any behavior VIA cannot represent */
};

struct vk_binding {
    enum vk_beh beh;
    uint32_t p1;
    uint32_t p2;
};

#define VK_QMK_KB0 0x7E00 /* first custom keycode */
#define VK_QMK_MAX_LAYER 32

/* Number of custom keycodes (custom_keycodes.def), including the placeholder. */
extern const uint8_t vk_custom_count;

/* VIA keycode for a ZMK binding. Anything with no equivalent maps to the
 * placeholder custom keycode, never to a wrong key. */
uint16_t vk_to_qmk(const struct vk_binding *b);

/* ZMK binding for a VIA keycode. False if ZMK cannot express it (the caller
 * leaves the key unchanged). The placeholder is not writable. */
bool vk_from_qmk(uint16_t qmk, struct vk_binding *out);

#endif /* VIA_XLAT_H */
