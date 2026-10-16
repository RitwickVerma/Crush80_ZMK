#include "led_map.h"

#define RRGB_KEYS  88
#define RRGB_LEDS  154

/* keymap position -> LED index, from the stock firmware's switch-to-LED table
 * (official v1.04 image, inner offset 0x1B90C: 6 x 16 bytes, matrix index ->
 * LED slot, 0xFF = no switch) read through the matrix transform in
 * crush80.dts. The chain runs row by row, left to right, with four slots that
 * have no switch of their own: 52 (second Caps Lock LED), 64 (Enter area, the
 * ISO # position) and 82 / 84 (either side of the Space LED, 83). Slots 0, 1
 * and 52 have been checked on hardware by the Wobkey_Crush_80_Patched_Firmware
 * project (Esc, F1, Caps Lock). */
static const uint8_t pos_to_led[RRGB_KEYS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 63, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 77, 78, 79, 80, 81, 83, 85, 86, 87,
    88, 89, 90, 91,
};

/* Keys lit by more than one LED: (keymap position, extra LED). */
static const uint8_t extra_leds[][2] = {
    {51, 52},  /* Caps Lock */
    {63, 64},  /* Enter */
    {80, 82},  /* Space, left */
    {80, 84},  /* Space, right */
};

/* LED index -> normalized physical XY (x 0..255, y 0..98). Key LEDs (0-91)
 * use the key centres from crush80_layout in crush80.dts; the side bar and
 * logo positions (92-153) are approximate. */
static const struct led_xy led_positions[RRGB_LEDS] = {
    {8,8},
    {26,8},
    {40,8},
    {54,8},
    {68,8},
    {85,8},
    {99,8},
    {113,8},
    {128,8},
    {145,8},
    {159,8},
    {173,8},
    {187,8},
    {205,8},
    {219,8},
    {233,8},
    {247,8},
    {8,26},
    {22,26},
    {36,26},
    {50,26},
    {64,26},
    {78,26},
    {92,26},
    {106,26},
    {120,26},
    {135,26},
    {149,26},
    {163,26},
    {177,26},
    {198,26},
    {219,26},
    {233,26},
    {247,26},
    {12,42},
    {29,42},
    {43,42},
    {57,42},
    {71,42},
    {85,42},
    {99,42},
    {113,42},
    {128,42},
    {142,42},
    {156,42},
    {170,42},
    {184,42},
    {201,42},
    {219,42},
    {233,42},
    {247,42},
    {13,58},
    {13,58},
    {33,58},
    {47,58},
    {61,58},
    {75,58},
    {89,58},
    {103,58},
    {117,58},
    {131,58},
    {145,58},
    {159,58},
    {173,58},
    {196,58},
    {196,58},
    {17,74},
    {40,74},
    {54,74},
    {68,74},
    {82,74},
    {96,74},
    {110,74},
    {124,74},
    {138,74},
    {152,74},
    {166,74},
    {185,74},
    {233,74},
    {10,90},
    {27,90},
    {45,90},
    {68,90},
    {98,90},
    {127,90},
    {150,90},
    {168,90},
    {185,90},
    {203,90},
    {219,90},
    {233,90},
    {247,90},
    {0,0},
    {0,5},
    {0,9},
    {0,14},
    {0,19},
    {0,23},
    {0,28},
    {0,33},
    {0,37},
    {0,42},
    {0,47},
    {0,51},
    {0,56},
    {0,61},
    {0,65},
    {0,70},
    {0,75},
    {0,79},
    {0,84},
    {0,89},
    {0,93},
    {0,98},
    {254,0},
    {254,5},
    {254,9},
    {254,14},
    {254,19},
    {254,23},
    {254,28},
    {254,33},
    {254,37},
    {254,42},
    {254,47},
    {254,51},
    {254,56},
    {254,61},
    {254,65},
    {254,70},
    {254,75},
    {254,79},
    {254,84},
    {254,89},
    {254,93},
    {254,98},
    {0,0},
    {32,0},
    {64,0},
    {95,0},
    {127,0},
    {159,0},
    {190,0},
    {222,0},
    {254,0},
    {0,98},
    {32,98},
    {64,98},
    {95,98},
    {127,98},
    {159,98},
    {190,98},
    {222,98},
    {254,98},
};

const struct led_xy *rrgb_led_xy = led_positions;

const struct led_xy *rrgb_xy_for_position(uint32_t position) {
    if (position >= RRGB_KEYS) {
        return (const struct led_xy *)0;
    }
    return &led_positions[pos_to_led[position]];
}

int rrgb_led_for_position(uint32_t position) {
    if (position >= RRGB_KEYS) {
        return -1;
    }
    return pos_to_led[position];
}

uint8_t rrgb_leds_for_position(uint32_t position, uint8_t out[RRGB_MAX_LEDS_PER_KEY]) {
    if (position >= RRGB_KEYS) {
        return 0;
    }
    uint8_t n = 0;
    out[n++] = pos_to_led[position];
    for (unsigned i = 0; i < sizeof(extra_leds) / sizeof(extra_leds[0]); i++) {
        if (extra_leds[i][0] == position && n < RRGB_MAX_LEDS_PER_KEY) {
            out[n++] = extra_leds[i][1];
        }
    }
    return n;
}
