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

    /* M0-M15 round trip and stay inside their range */
    for (unsigned m = 0; m < VK_MACRO_COUNT; m++) {
        struct vk_binding mb = {VK_MACRO, m, 0}, back;

        if (vk_to_qmk(&mb) != VK_QMK_MACRO0 + m || !vk_from_qmk(VK_QMK_MACRO0 + m, &back) || back.beh != VK_MACRO ||
            back.p1 != m) {
            printf("macro %u does not round trip\n", m);
            bad++;
        }
    }
    struct vk_binding big_macro = {VK_MACRO, VK_MACRO_COUNT, 0};

    if (vk_to_qmk(&big_macro) != placeholder || vk_from_qmk(VK_QMK_MACRO0 + VK_MACRO_COUNT, &w)) {
        printf("macro numbers past the last one must not map\n");
        bad++;
    }

    /* ASCII -> key for macro text: every printable character has a key, no two
     * characters share (usage, shift), and the common ones are what a US keyboard types. */
    uint8_t seen[256][2] = {{0}};

    for (int c = 0x20; c < 0x7F; c++) {
        uint8_t u;
        bool sh;

        if (!vk_ascii_to_usage((char)c, &u, &sh)) {
            printf("printable 0x%02x has no key\n", c);
            bad++;
            continue;
        }
        if (seen[u][sh]++) {
            printf("0x%02x collides with another character\n", c);
            bad++;
        }
    }
    const struct { char c; uint8_t u; bool sh; } known[] = {
        {'a', 0x04, false}, {'A', 0x04, true}, {'z', 0x1D, false}, {'1', 0x1E, false}, {'!', 0x1E, true},
        {'0', 0x27, false}, {')', 0x27, true},  {'@', 0x1F, true},  {'#', 0x20, true}, {'(', 0x26, true},
        {' ', 0x2C, false}, {'\n', 0x28, false}, {'\t', 0x2B, false}, {'_', 0x2D, true}, {'+', 0x2E, true},
        {'{', 0x2F, true},  {'|', 0x31, true},  {':', 0x33, true},  {'"', 0x34, true}, {'~', 0x35, true},
        {'<', 0x36, true},  {'?', 0x38, true},  {'/', 0x38, false}, {'\\', 0x31, false},
    };
    for (unsigned i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        uint8_t u;
        bool sh;

        if (!vk_ascii_to_usage(known[i].c, &u, &sh) || u != known[i].u || sh != known[i].sh) {
            printf("'%c' maps to the wrong key\n", known[i].c);
            bad++;
        }
    }
    uint8_t u;
    bool sh;

    if (vk_ascii_to_usage(0x01, &u, &sh) || vk_ascii_to_usage((char)0x80, &u, &sh)) {
        printf("control and non-ASCII bytes must not map\n");
        bad++;
    }

    printf("%d VIA keycodes supported, %s\n", supported, bad ? "FAILED" : "all round-trip");
    return bad != 0;
}
