/* Host test for via_xlat.c: every VIA keycode ZMK accepts must translate
 * back to itself, and unrepresentable ZMK bindings must map to the read-only
 * placeholder, never to another key. Run with run_via_xlat_test.sh. */
#include <stdio.h>

#include "via_xlat.h"

int main(void) {
    int bad = 0, supported = 0;

    for (uint32_t q = 0; q <= 0xFFFF; q++) {
        struct vk_binding b;

        if (!vk_from_qmk(q, &b)) {
            continue;
        }
        supported++;
        if (vk_to_qmk(&b) != q) {
            printf("round trip fails for 0x%04x\n", q);
            bad++;
        }
    }

    struct vk_binding other = {VK_OTHER, 0, 0}, w;
    uint16_t placeholder = vk_to_qmk(&other);

    if (vk_from_qmk(placeholder, &w) || placeholder != VK_QMK_KB0 + vk_custom_count - 1) {
        printf("placeholder must be the last custom key and read-only\n");
        bad++;
    }

    const struct vk_binding unrepresentable[] = {
        {VK_KP, (0x11u << 24) | (0x07 << 16) | 0x04, 0}, /* left Ctrl + right Shift */
        {VK_KP, (0x0C << 16) | 0x1234, 0},                 /* unknown consumer usage */
        {VK_MO, VK_QMK_MAX_LAYER, 0},                      /* layer out of range */
        {VK_LT, 16, (0x07 << 16) | 0x04},
    };
    for (unsigned i = 0; i < sizeof(unrepresentable) / sizeof(unrepresentable[0]); i++) {
        if (vk_to_qmk(&unrepresentable[i]) != placeholder) {
            printf("binding %u should map to the placeholder\n", i);
            bad++;
        }
    }

    printf("%d VIA keycodes supported, %s\n", supported, bad ? "FAILED" : "all round-trip");
    return bad != 0;
}
