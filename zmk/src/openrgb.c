/*
 * Copyright 2025
 * SPDX-License-Identifier: MIT
 *
 * openrgb.c — Direct OpenRGB support over a Raw HID interface (HID_1).
 *
 * Implements the QMK OpenRGB protocol (Kasper24/qmk-openrgb), revision 0x0C,
 * which is what OpenRGB's QMKOpenRGB controller speaks:
 *
 *   - Raw HID interface: usage page 0xFF60, usage 0x61
 *   - 64-byte reports, NO report ID (the host prepends a 0x00 report-id byte
 *     that the Linux hidraw layer strips before the packet reaches us)
 *   - command ID in byte 0 of each report
 *
 * The rainy_rgb engine's "host direct pixel mode" is reused for per-LED
 * control: OpenRGB's direct mode writes straight into the engine's host pixel
 * buffer, while the functional overlays (CapsLock / battery) still render on
 * top.  Non-direct modes are mapped onto the closest rainy_rgb effect.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <string.h>

#include "rainy_rgb/engine.h"
#include "rainy_rgb/effects.h"
#include "rainy_rgb/led_map.h"   /* RRGB_KEY_LED_COUNT, rrgb_led_xy */
#include "rainy_rgb/led_map.h"

LOG_MODULE_REGISTER(openrgb, CONFIG_LOG_DEFAULT_LEVEL);

#define OPENRGB_EP_SIZE        64
#define OPENRGB_LED_COUNT      154
#define OPENRGB_PROTOCOL_VERSION 0x0C

/* Command IDs (stable across QMK OpenRGB protocol revisions). */
enum {
    CMD_GET_PROTOCOL_VERSION = 1,
    CMD_GET_QMK_VERSION      = 2,
    CMD_GET_DEVICE_INFO      = 3,
    CMD_GET_MODE_INFO        = 4,
    CMD_GET_LED_INFO         = 5,
    CMD_GET_ENABLED_MODES    = 6,
    CMD_SET_MODE             = 7,
    CMD_SET_SINGLE_LED       = 8,
    CMD_SET_LEDS             = 9,
};

/* Response status codes. */
enum {
    RSP_FAILURE        = 25,
    RSP_SUCCESS        = 50,
    RSP_END_OF_MESSAGE = 100,
    RSP_UNHANDLED      = 0xFF, /* QMK/VIA id_unhandled */
};

/* QMK OpenRGB mode enum values (what we advertise in GET_ENABLED_MODES). */
enum {
    MODE_OPENRGB_DIRECT    = 1,
    MODE_SOLID_COLOR       = 2,
    MODE_CYCLE_ALL         = 13,
    MODE_CYCLE_LEFT_RIGHT  = 14,
    MODE_RAINBOW_BEACON    = 22,
    MODE_RAINDROPS         = 24,
    MODE_JELLYBEAN_RAINDROPS = 25,
    MODE_TYPING_HEATMAP    = 29,
    MODE_SOLID_REACTIVE    = 32,
    MODE_SPLASH            = 39,
};

/*
 * Raw HID report descriptor — matches QMK's RAW_ENABLE descriptor with
 * RAW_EPSIZE = 64.  No Report ID item: reports are 64 bytes and the Linux
 * hidraw layer strips the leading 0x00 that OpenRGB prefixes every write
 * with (it is treated as "no report ID" by usbhid_output_report).
 */
static const uint8_t openrgb_report_desc[] = {
    0x06, 0x60, 0xFF,            /* Usage Page (Vendor Defined 0xFF60) */
    0x09, 0x61,                  /* Usage (Vendor Defined 0x61) */
    0xA1, 0x01,                  /* Collection (Application) */
    0x09, 0x62,                  /*   Usage (Vendor Defined 0x62) — data to host */
    0x15, 0x00,                  /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,            /*   Logical Maximum (255) */
    0x95, 0x40,                  /*   Report Count (64) */
    0x75, 0x08,                  /*   Report Size (8) */
    0x81, 0x02,                  /*   Input (Data,Var,Abs) */
    0x09, 0x63,                  /*   Usage (Vendor Defined 0x63) — data from host */
    0x15, 0x00,                  /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,            /*   Logical Maximum (255) */
    0x95, 0x40,                  /*   Report Count (64) */
    0x75, 0x08,                  /*   Report Size (8) */
    0x91, 0x02,                  /*   Output (Data,Var,Abs) */
    0xC0,                        /* End Collection */
};

/* OpenRGB assigns each enabled mode a sequential value in the order its
 * RGBController_QMKOpenRGBRevB constructor tests for them.  The "Direct"
 * mode is inserted at UI index 0 and gets the LAST sequential value.  This
 * table maps that sequential value back to a rainy_rgb effect index. */
#define OPENRGB_MODE_DIRECT  10   /* sequential value of the Direct mode */

static const int8_t mode_to_effect[OPENRGB_MODE_DIRECT] = {
    -1,  /* [0] unused */
    0,   /* 1: Solid Color      -> solid */
    1,   /* 2: Cycle All        -> rainbow */
    8,   /* 3: Cycle Left Right -> wave */
    4,   /* 4: Rainbow Beacon   -> comet */
    9,   /* 5: Raindrops        -> rain */
    3,   /* 6: Jellybean        -> twinkle */
    10,  /* 7: Typing Heatmap   -> heatmap */
    6,   /* 8: Solid Reactive   -> reactive */
    7,   /* 9: Splash           -> ripple */
};

static const struct device *openrgb_dev;
static uint8_t openrgb_mode = 1; /* current OpenRGB sequential mode (1=solid) */

/* OUT: signalled from ISR when a report arrives.  Max 4 lets a burst of
 * SET_LEDS packets queue before the handler thread drains them. */
static K_SEM_DEFINE(openrgb_out_sem, 0, 4);
/* IN: serialises response writes — one transfer in flight at a time. */
static K_SEM_DEFINE(openrgb_in_sem, 1, 1);

static void in_ready_cb(const struct device *dev) {
    ARG_UNUSED(dev);
    k_sem_give(&openrgb_in_sem);
}

static void out_ready_cb(const struct device *dev) {
    ARG_UNUSED(dev);
    k_sem_give(&openrgb_out_sem);
}

static const struct hid_ops openrgb_ops = {
    .int_in_ready = in_ready_cb,
    .int_out_ready = out_ready_cb,
};

/* Send a 64-byte response.  response[63] carries END_OF_MESSAGE. */
static void openrgb_send(uint8_t *response) {
    response[OPENRGB_EP_SIZE - 1] = RSP_END_OF_MESSAGE;
    /* A bus reset drops an in-flight IN transfer without calling
     * in_ready_cb, so a timeout here means the token was lost, not that a
     * transfer is still pending: write anyway. A successful write gives the
     * token back through in_ready_cb, which resynchronises the semaphore. */
    bool have_token = k_sem_take(&openrgb_in_sem, K_MSEC(50)) == 0;
    int err = hid_int_ep_write(openrgb_dev, response, OPENRGB_EP_SIZE, NULL);

    if (err && have_token) {
        k_sem_give(&openrgb_in_sem);
    }
}

static void cmd_get_protocol_version(uint8_t *rsp) {
    rsp[1] = OPENRGB_PROTOCOL_VERSION;
}

static void cmd_get_qmk_version(uint8_t *rsp) {
    static const char *ver = "0.1.0";
    for (size_t i = 0; ver[i] && i < (OPENRGB_EP_SIZE - 2); i++) {
        rsp[1 + i] = (uint8_t)ver[i];
    }
}

static void cmd_get_device_info(uint8_t *rsp) {
    static const char *name = "Crush 80";
    static const char *vendor = "Wobkey";

    rsp[1] = OPENRGB_LED_COUNT;  /* total LEDs */
    rsp[2] = OPENRGB_LED_COUNT;  /* LEDs with empty space (none reserved) */

    uint8_t idx = 3;
    for (size_t i = 0; name[i] && idx < (OPENRGB_EP_SIZE - 2); i++) {
        rsp[idx++] = (uint8_t)name[i];
    }
    rsp[idx++] = 0;
    for (size_t i = 0; vendor[i] && idx < (OPENRGB_EP_SIZE - 2); i++) {
        rsp[idx++] = (uint8_t)vendor[i];
    }
}

static void cmd_get_mode_info(uint8_t *rsp) {
    struct rrgb_persist p;
    rrgb_get_persist(&p);

    /* Fn+RGB on the keyboard leaves Direct mode behind the host's back:
     * report the effect that is actually running. */
    if (openrgb_mode == OPENRGB_MODE_DIRECT && !rrgb_host_active()) {
        openrgb_mode = 1;
        for (uint8_t m = 1; m < OPENRGB_MODE_DIRECT; m++) {
            if (mode_to_effect[m] == p.effect) {
                openrgb_mode = m;
                break;
            }
        }
    }

    rsp[1] = openrgb_mode;
    rsp[2] = p.speed;
    rsp[3] = p.hue;
    rsp[4] = p.sat;
    rsp[5] = p.val;
}

/* HID usage code for each key LED (0-91), indexed by LED slot. Built from
 * crush80.keymap's base-layer bindings through pos_to_led in rainy_rgb/led_map.c
 * (the four extra slots under Caps Lock, Enter and Space repeat that key's
 * code). OpenRGB uses this byte to look up a key name (e.g. 0x04 = "A");
 * Fn reports 0 (no name). */
static const uint8_t led_keycodes[RRGB_KEY_LED_COUNT] = {
    0x29, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x68, 0x46, 0x47, 0x48, 0x35, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x49, 0x4A, 0x4B, 0x2B, 0x14,
    0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31,
    0x4C, 0x4D, 0x4E, 0x39, 0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D,
    0x0E, 0x0F, 0x33, 0x34, 0x28, 0x28, 0xE1, 0x1D, 0x1B, 0x06, 0x19, 0x05,
    0x11, 0x10, 0x36, 0x37, 0x38, 0xE5, 0x52, 0xE0, 0xE3, 0xE2, 0x2C, 0x2C,
    0x2C, 0xE6, 0x00, 0xE7, 0xE4, 0x50, 0x51, 0x4F,
};

static void cmd_get_led_info(uint8_t *rsp, const uint8_t *data) {
    uint8_t first = data[1];
    uint8_t count = data[2];

    /* 7 bytes per LED after the command byte: at most 8 fit in 64 bytes.
     * Capping the loop (not just skipping) keeps o from wrapping. */
    count = MIN(count, (OPENRGB_EP_SIZE - 8) / 7 + 1);

    for (uint8_t i = 0; i < count; i++) {
        uint16_t led = (uint16_t)first + i;
        uint8_t o = 1 + i * 7;

        if (led >= OPENRGB_LED_COUNT) {
            rsp[o + 2] = RSP_FAILURE; /* flags position */
            continue;
        }

        const struct led_xy *xy = &rrgb_led_xy[led];
        uint8_t flags = (led < RRGB_KEY_LED_COUNT) ? 0x04 : 0x02; /* keylight | underglow */

        rsp[o + 0] = xy->x;
        rsp[o + 1] = xy->y;
        rsp[o + 2] = flags;
        rsp[o + 3] = 0; /* r (read back by OpenRGB; we have no per-LED cache) */
        rsp[o + 4] = 0; /* g */
        rsp[o + 5] = 0; /* b */
        rsp[o + 6] = (led < RRGB_KEY_LED_COUNT) ? led_keycodes[led] : 0; /* keycode */
    }
}

static void cmd_get_enabled_modes(uint8_t *rsp) {
    static const uint8_t modes[] = {
        MODE_SOLID_COLOR,
        MODE_CYCLE_ALL,
        MODE_CYCLE_LEFT_RIGHT,
        MODE_RAINBOW_BEACON,
        MODE_RAINDROPS,
        MODE_JELLYBEAN_RAINDROPS,
        MODE_TYPING_HEATMAP,
        MODE_SOLID_REACTIVE,
        MODE_SPLASH,
        /* OpenRGB only offers its per-LED "Direct" mode when the device lists
         * it; it is tested last, so it gets sequential value 10
         * (OPENRGB_MODE_DIRECT). */
        MODE_OPENRGB_DIRECT,
    };
    for (size_t i = 0; i < sizeof(modes); i++) {
        rsp[1 + i] = modes[i];
    }
    /* rsp[1 + sizeof(modes)] == 0 (memset) terminates the list */
}

static void apply_effect(uint8_t effect, uint8_t hue, uint8_t sat, uint8_t val,
                         uint8_t speed, bool save) {
    struct rrgb_persist p;
    p.version = 1;
    p.on = true;
    p.effect = effect;
    p.hue = hue;
    p.sat = sat;
    p.val = val;
    p.speed = speed;
    rrgb_set_persist(&p);
    if (save) {
        rrgb_request_save();
    }
}

static void cmd_set_mode(uint8_t *rsp, const uint8_t *data) {
    uint8_t hue = data[1];
    uint8_t sat = data[2];
    uint8_t val = data[3];
    uint8_t mode = data[4];
    uint8_t speed = data[5];
    bool save = (data[6] == 1);

    if (mode == OPENRGB_MODE_DIRECT) {
        /* Direct mode: the host drives every LED from now on. Start black
         * so a partial update lights exactly what the host asked for. */
        rrgb_host_clear();
        rrgb_host_fill(0, 0, 0);
        openrgb_mode = mode;
        rsp[OPENRGB_EP_SIZE - 2] = RSP_SUCCESS;
        return;
    }

    if (mode < 1 || mode >= OPENRGB_MODE_DIRECT || mode_to_effect[mode] < 0) {
        rsp[OPENRGB_EP_SIZE - 2] = RSP_FAILURE;
        return;
    }

    /* Leaving direct mode: restore the effect layer. */
    rrgb_host_clear();
    apply_effect((uint8_t)mode_to_effect[mode], hue, sat, val, speed, save);
    openrgb_mode = mode;
    rsp[OPENRGB_EP_SIZE - 2] = RSP_SUCCESS;
}

static void cmd_set_single_led(uint8_t *rsp, const uint8_t *data) {
    uint16_t led = data[1];
    uint8_t r = data[2];
    uint8_t g = data[3];
    uint8_t b = data[4];

    if (led >= OPENRGB_LED_COUNT) {
        rsp[OPENRGB_EP_SIZE - 2] = RSP_FAILURE;
        return;
    }

    rrgb_host_set_led(led, r, g, b);
    rsp[OPENRGB_EP_SIZE - 2] = RSP_SUCCESS;
}

/* Bulk LED write: data[1]=first, data[2]=count, then RGB triples. No response
 * (matches the canonical firmware — throughput beats acking here). */
static void cmd_set_leds(const uint8_t *data, uint32_t len) {
    if (len < 3) {
        return;
    }

    uint16_t first = data[1];
    /* The host picks count; only as many triples as the packet holds (20
     * in 64 bytes) are real data. */
    uint8_t count = MIN(data[2], (len - 3) / 3);

    for (uint8_t i = 0; i < count; i++) {
        uint16_t led = first + i;
        if (led >= OPENRGB_LED_COUNT) {
            continue;
        }
        rrgb_host_set_led(led, data[3 + i * 3], data[4 + i * 3], data[5 + i * 3]);
    }
}

static void openrgb_handle(const uint8_t *data, uint32_t len) {
    uint8_t response[OPENRGB_EP_SIZE];

    if (len < 1) {
        return;
    }

    uint8_t cmd = data[0];

    /* OpenRGB always sends full 64-byte packets. This interface has the same
     * identity as the VIA one (usage page 0xFF60, usage 0x61), so VIA's
     * configurator also opens it and probes with 32-byte packets. Answer those
     * with QMK's "unhandled" id so VIA drops this device instead of treating
     * our reply as a keyboard of protocol 0x0C00, and never act on them. */
    if (len < OPENRGB_EP_SIZE) {
        memset(response, 0, sizeof(response));
        response[0] = RSP_UNHANDLED;
        response[1] = cmd;
        openrgb_send(response);
        return;
    }

    if (cmd == CMD_SET_LEDS) {
        cmd_set_leds(data, len);
        return; /* no response */
    }

    memset(response, 0, sizeof(response));
    response[0] = cmd; /* echo command ID */

    switch (cmd) {
    case CMD_GET_PROTOCOL_VERSION:
        cmd_get_protocol_version(response);
        break;
    case CMD_GET_QMK_VERSION:
        cmd_get_qmk_version(response);
        break;
    case CMD_GET_DEVICE_INFO:
        cmd_get_device_info(response);
        break;
    case CMD_GET_MODE_INFO:
        cmd_get_mode_info(response);
        break;
    case CMD_GET_LED_INFO:
        cmd_get_led_info(response, data);
        break;
    case CMD_GET_ENABLED_MODES:
        cmd_get_enabled_modes(response);
        break;
    case CMD_SET_MODE:
        cmd_set_mode(response, data);
        break;
    case CMD_SET_SINGLE_LED:
        cmd_set_single_led(response, data);
        break;
    default:
        response[OPENRGB_EP_SIZE - 2] = RSP_FAILURE;
        break;
    }

    openrgb_send(response);
}

K_THREAD_STACK_DEFINE(openrgb_stack, 1024);
static struct k_thread openrgb_thread;

static void openrgb_thread_fn(void *a, void *b, void *c) {
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    uint8_t buf[OPENRGB_EP_SIZE];

    for (;;) {
        k_sem_take(&openrgb_out_sem, K_FOREVER);

        /* Drain every packet that has arrived; a new packet can land between
         * the sem signal and this read, so loop until the staging buffer is
         * empty rather than reading exactly once. */
        for (;;) {
            uint32_t bytes = 0;
            int err = hid_int_ep_read(openrgb_dev, buf, sizeof(buf), &bytes);
            if (err || bytes == 0) {
                break;
            }
            openrgb_handle(buf, bytes);
        }
    }
}

static int openrgb_init(void) {
    openrgb_dev = device_get_binding("HID_1");
    if (openrgb_dev == NULL) {
        LOG_ERR("Unable to locate HID_1 (raw HID) device");
        return -EINVAL;
    }

    usb_hid_register_device(openrgb_dev, openrgb_report_desc,
                            sizeof(openrgb_report_desc), &openrgb_ops);
    usb_hid_init(openrgb_dev);

    k_thread_create(&openrgb_thread, openrgb_stack, K_THREAD_STACK_SIZEOF(openrgb_stack),
                    openrgb_thread_fn, NULL, NULL, NULL, 10, 0, K_NO_WAIT);
    k_thread_name_set(&openrgb_thread, "openrgb");

    LOG_INF("OpenRGB raw HID ready (%d LEDs, protocol 0x%02X)",
            OPENRGB_LED_COUNT, OPENRGB_PROTOCOL_VERSION);
    return 0;
}

SYS_INIT(openrgb_init, APPLICATION, 94);
