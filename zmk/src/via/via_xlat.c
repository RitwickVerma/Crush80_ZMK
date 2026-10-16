#include "via_xlat.h"

#include <dt-bindings/zmk/keys.h>

#define KB_PAGE   0x07
#define CONS_PAGE 0x0C

#define QMK_TRNS      0x0001
#define QMK_KEY_MIN   0x04
#define QMK_KEY_MAX   0xA4 /* last keyboard-page key QMK maps 1:1 */
#define QMK_TO        0x5200
#define QMK_MOMENTARY 0x5220
#define QMK_TOGGLE    0x5260
#define QMK_LAYER_TAP 0x4000

/* QMK's "basic" consumer keys and the ZMK usages they stand for. */
static const struct {
    uint16_t qmk;
    uint32_t zmk;
} consumer[] = {
    {0xA8, C_MUTE},   {0xA9, C_VOL_UP}, {0xAA, C_VOL_DN},  {0xAB, C_NEXT}, {0xAC, C_PREV},
    {0xAD, C_STOP},   {0xAE, C_PP},     {0xBD, C_BRI_UP}, {0xBE, C_BRI_DN},
};

static const struct {
    const char *name; /* unused at run time; documents the table */
    enum vk_beh beh;
    uint32_t p1;
    uint32_t p2;
} custom[] = {
#define VK_CUSTOM(name, title, beh, p1, p2) {name, beh, p1, p2},
#include "custom_keycodes.def"
#undef VK_CUSTOM
};

const uint8_t vk_custom_count = sizeof(custom) / sizeof(custom[0]);

#define PLACEHOLDER (VK_QMK_KB0 + sizeof(custom) / sizeof(custom[0]) - 1)

/* Keyboard-page usages VIA names (bit n = usage n), from VIA's protocol-12
 * keycode dictionary (the-via/app, src/utils/key-to-byte/v12.ts). Other usages are valid HID but VIA could only show them as raw hex,
 * so they are treated as "no VIA equivalent". */
static const uint32_t via_known[8] = {
    0xFFFFFFF0, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF,
    0x3FFFFFFC, 0x0000001F, 0x00000000, 0x000000FF,
};

static bool is_basic_key(uint32_t id) {
    return id <= 0xFF && (via_known[id / 32] & (1u << (id % 32)));
}

/* ZMK modifier byte (8 flags) <-> QMK 5-bit field (4 flags + right-hand bit).
 * QMK cannot say "left Ctrl and right Shift" at once. */
static bool mods_to_qmk(uint32_t zmk, uint32_t *qmk) {
    uint32_t left = zmk & 0x0F, right = (zmk >> 4) & 0x0F;

    if (left && right) {
        return false;
    }
    *qmk = right ? (0x10 | right) : left;
    return true;
}

static uint32_t mods_from_qmk(uint32_t qmk) {
    return (qmk & 0x10) ? ((qmk & 0x0F) << 4) : (qmk & 0x0F);
}

/* ZMK keycode -> basic QMK keycode (low 8 bits) and modifiers; false if the
 * usage has no QMK equivalent. */
static bool kc_to_qmk(uint32_t kc, uint16_t *key, uint32_t *mods) {
    uint32_t page = (kc >> 16) & 0xFF, id = kc & 0xFFFF;

    *mods = (kc >> 24) & 0xFF;
    if (page == 0) {
        page = KB_PAGE;
    }
    if (page == KB_PAGE && is_basic_key(id)) {
        *key = id;
        return true;
    }
    if (page == CONS_PAGE && *mods == 0) {
        for (unsigned i = 0; i < sizeof(consumer) / sizeof(consumer[0]); i++) {
            if (((consumer[i].zmk >> 16) & 0xFF) == page && (consumer[i].zmk & 0xFFFF) == id) {
                *key = consumer[i].qmk;
                return true;
            }
        }
    }
    return false;
}

uint16_t vk_to_qmk(const struct vk_binding *b) {
    uint16_t key;
    uint32_t mods, q;

    switch (b->beh) {
    case VK_NONE:
        return 0;
    case VK_TRANS:
        return QMK_TRNS;
    case VK_KP:
        if (!kc_to_qmk(b->p1, &key, &mods) || !mods_to_qmk(mods, &q)) {
            break;
        }
        /* Modifiers only combine with plain keys, not with consumer codes. */
        if (q && !is_basic_key(key)) {
            break;
        }
        return key | (q << 8);
    case VK_MO:
        return b->p1 < VK_QMK_MAX_LAYER ? QMK_MOMENTARY | b->p1 : PLACEHOLDER;
    case VK_TOG:
        return b->p1 < VK_QMK_MAX_LAYER ? QMK_TOGGLE | b->p1 : PLACEHOLDER;
    case VK_TO:
        return b->p1 < VK_QMK_MAX_LAYER ? QMK_TO | b->p1 : PLACEHOLDER;
    case VK_MACRO:
        return b->p1 < VK_MACRO_COUNT ? VK_QMK_MACRO0 + b->p1 : PLACEHOLDER;
    case VK_LT:
        if (b->p1 < 16 && kc_to_qmk(b->p2, &key, &mods) && mods == 0 && key >= QMK_KEY_MIN &&
            key <= QMK_KEY_MAX && is_basic_key(key)) {
            return QMK_LAYER_TAP | (b->p1 << 8) | key;
        }
        break;
    default:
        for (unsigned i = 0; i < sizeof(custom) / sizeof(custom[0]); i++) {
            if (custom[i].beh == b->beh && custom[i].p1 == b->p1 && custom[i].p2 == b->p2) {
                return VK_QMK_KB0 + i;
            }
        }
        break;
    }
    return PLACEHOLDER;
}

static bool basic_to_zmk(uint16_t key, uint32_t mods, struct vk_binding *out) {
    out->beh = VK_KP;
    out->p2 = 0;
    if (is_basic_key(key)) {
        out->p1 = (mods << 24) | (KB_PAGE << 16) | key;
        return true;
    }
    for (unsigned i = 0; i < sizeof(consumer) / sizeof(consumer[0]); i++) {
        if (consumer[i].qmk == key && mods == 0) {
            out->p1 = consumer[i].zmk;
            return true;
        }
    }
    return false;
}

bool vk_from_qmk(uint16_t qmk, struct vk_binding *out) {
    out->p1 = out->p2 = 0;

    if (qmk <= 0x00FF) {
        if (qmk == 0) {
            out->beh = VK_NONE;
            return true;
        }
        if (qmk == QMK_TRNS) {
            out->beh = VK_TRANS;
            return true;
        }
        return basic_to_zmk(qmk, 0, out);
    }
    if (qmk < 0x2000) { /* modifiers + basic key */
        uint16_t key = qmk & 0xFF;
        uint32_t mods = mods_from_qmk((qmk >> 8) & 0x1F);

        /* A right-hand flag with no modifier (0x10xx) is not a real code. */
        return mods && is_basic_key(key) && key <= QMK_KEY_MAX && basic_to_zmk(key, mods, out);
    }
    if (qmk >= QMK_LAYER_TAP && qmk < 0x5000) { /* LT(layer, key) */
        uint16_t key = qmk & 0xFF;

        if (!is_basic_key(key) || key > QMK_KEY_MAX) {
            return false;
        }
        out->beh = VK_LT;
        out->p1 = (qmk >> 8) & 0x0F;
        out->p2 = (KB_PAGE << 16) | key;
        return true;
    }
    if (qmk >= QMK_TO && qmk < QMK_TO + VK_QMK_MAX_LAYER) {
        out->beh = VK_TO;
        out->p1 = qmk - QMK_TO;
        return true;
    }
    if (qmk >= QMK_MOMENTARY && qmk < QMK_MOMENTARY + VK_QMK_MAX_LAYER) {
        out->beh = VK_MO;
        out->p1 = qmk - QMK_MOMENTARY;
        return true;
    }
    if (qmk >= QMK_TOGGLE && qmk < QMK_TOGGLE + VK_QMK_MAX_LAYER) {
        out->beh = VK_TOG;
        out->p1 = qmk - QMK_TOGGLE;
        return true;
    }
    if (qmk >= VK_QMK_MACRO0 && qmk < VK_QMK_MACRO0 + VK_MACRO_COUNT) {
        out->beh = VK_MACRO;
        out->p1 = qmk - VK_QMK_MACRO0;
        return true;
    }
    if (qmk >= VK_QMK_KB0 && qmk < PLACEHOLDER) {
        unsigned i = qmk - VK_QMK_KB0;

        out->beh = custom[i].beh;
        out->p1 = custom[i].p1;
        out->p2 = custom[i].p2;
        return true;
    }
    return false;
}

bool vk_ascii_to_usage(char c, uint8_t *usage, bool *shift) {
    static const char shifted_digits[] = ")!@#$%^&*(";   /* 0-9 */
    static const struct { char plain, shifted; uint8_t usage; } punct[] = {
        {'-', '_', 0x2D}, {'=', '+', 0x2E}, {'[', '{', 0x2F}, {']', '}', 0x30}, {'\\', '|', 0x31},
        {';', ':', 0x33}, {'\'', '"', 0x34}, {'`', '~', 0x35}, {',', '<', 0x36}, {'.', '>', 0x37},
        {'/', '?', 0x38},
    };

    *shift = false;
    if (c >= 'a' && c <= 'z') {
        *usage = 0x04 + (c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *usage = 0x04 + (c - 'A');
        *shift = true;
        return true;
    }
    if (c >= '1' && c <= '9') {
        *usage = 0x1E + (c - '1');
        return true;
    }
    if (c == '0') {
        *usage = 0x27;
        return true;
    }
    for (unsigned i = 0; i < sizeof(shifted_digits) - 1; i++) {
        if (c == shifted_digits[i]) {
            *usage = i ? 0x1D + i : 0x27;
            *shift = true;
            return true;
        }
    }
    for (unsigned i = 0; i < sizeof(punct) / sizeof(punct[0]); i++) {
        if (c == punct[i].plain || c == punct[i].shifted) {
            *usage = punct[i].usage;
            *shift = c == punct[i].shifted;
            return true;
        }
    }
    switch (c) {
    case '\n':
    case '\r':
        *usage = 0x28;
        return true;
    case 0x1B:
        *usage = 0x29;
        return true;
    case 0x08:
        *usage = 0x2A;
        return true;
    case '\t':
        *usage = 0x2B;
        return true;
    case ' ':
        *usage = 0x2C;
        return true;
    case 0x7F:
        *usage = 0x4C;
        return true;
    }
    return false;
}
