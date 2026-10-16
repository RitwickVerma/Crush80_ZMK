/*
 * Macros editable from VIA: a 1 KB buffer in VIA's own format, played by the
 * &vmacro behavior (M0-M15 in VIA).
 *
 * Format (VIA protocol 12): each macro ends with 0x00. Inside one:
 *   0x01 0x01 kc   tap key kc      0x01 0x02 kc   key down
 *   0x01 0x03 kc   key up          0x01 0x04 ddd| delay of ddd ms (ASCII digits)
 *   any other byte is a character to type (US layout).
 *
 * Timing follows ZMK's own macros: 30 ms taps, 15 ms between actions.
 */

#define DT_DRV_COMPAT crush80_behavior_via_macro

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

#include "via_macro.h"
#include "via_xlat.h"

LOG_MODULE_REGISTER(via_macro, CONFIG_LOG_DEFAULT_LEVEL);

#define TAP_MS 30
#define WAIT_MS 15
#define SAVE_DELAY K_SECONDS(5)
#define KB_PAGE 0x07
#define MAX_HELD 8
#define MAX_DELAY_DIGITS 5

static uint8_t buf[VIA_MACRO_SIZE];
static K_MUTEX_DEFINE(lock);

/* ---- storage -------------------------------------------------------------- */

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "buf") != 0 || len > sizeof(buf)) {
        return -ENOENT;
    }
    if (read_cb(cb_arg, buf, len) < 0) {
        return -EIO;
    }
    buf[sizeof(buf) - 1] = 0; /* a macro must always end */
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(via_macro, "via_macro", NULL, settings_set, NULL, NULL);

static void save_fn(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&lock, K_FOREVER);
    int err = settings_save_one("via_macro/buf", buf, sizeof(buf));

    k_mutex_unlock(&lock);
    if (err) {
        LOG_ERR("saving macros failed (%d)", err);
    }
}

static K_WORK_DELAYABLE_DEFINE(save_work, save_fn);

/* ---- playback --------------------------------------------------------------- */

static void play_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(play_work, play_fn);

static bool playing;
static uint16_t pos;
static bool tap_pending;
static uint32_t tap_encoded;
static uint32_t held[MAX_HELD];
static uint8_t held_count;

static void key_event(uint32_t encoded, bool pressed) {
    raise_zmk_keycode_state_changed_from_encoded(encoded, pressed, k_uptime_get());
}

static void held_add(uint32_t encoded) {
    if (held_count < MAX_HELD) {
        held[held_count++] = encoded;
    }
}

static void held_remove(uint32_t encoded) {
    for (uint8_t i = 0; i < held_count; i++) {
        if (held[i] == encoded) {
            held[i] = held[--held_count];
            return;
        }
    }
}

/* Release everything still down, so a macro cannot leave a key stuck. */
static void stop_playback(void) {
    k_work_cancel_delayable(&play_work);
    if (tap_pending) {
        key_event(tap_encoded, false);
        tap_pending = false;
    }
    while (held_count) {
        key_event(held[--held_count], false);
    }
    playing = false;
}

static uint32_t usage(uint8_t kc) {
    return (KB_PAGE << 16) | kc;
}

static void play_fn(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&lock, K_FOREVER);

    if (tap_pending) { /* second half of a tap or a typed character */
        key_event(tap_encoded, false);
        tap_pending = false;
        k_work_reschedule(&play_work, K_MSEC(WAIT_MS));
        goto out;
    }
    if (!playing || buf[pos] == 0) {
        stop_playback();
        goto out;
    }

    uint8_t c = buf[pos++];

    if (c == 0x01) {
        uint8_t action = buf[pos];

        if (action < 1 || action > 4 || pos + 1 >= sizeof(buf)) {
            stop_playback(); /* corrupt buffer */
            goto out;
        }
        pos++;
        if (action == 4) {
            uint32_t ms = 0;

            for (int i = 0; i < MAX_DELAY_DIGITS && buf[pos] >= '0' && buf[pos] <= '9'; i++) {
                ms = ms * 10 + (buf[pos++] - '0');
            }
            if (buf[pos] == '|') {
                pos++;
            }
            k_work_reschedule(&play_work, K_MSEC(MAX(ms, 1)));
            goto out;
        }
        uint32_t enc = usage(buf[pos++]);

        key_event(enc, action != 3);
        if (action == 1) {
            tap_encoded = enc;
            tap_pending = true;
            k_work_reschedule(&play_work, K_MSEC(TAP_MS));
        } else {
            action == 2 ? held_add(enc) : held_remove(enc);
            k_work_reschedule(&play_work, K_MSEC(WAIT_MS));
        }
    } else {
        uint8_t kc;
        bool shift;

        if (vk_ascii_to_usage(c, &kc, &shift)) {
            tap_encoded = usage(kc) | (shift ? (0x02u << 24) : 0); /* left shift */
            tap_pending = true;
            key_event(tap_encoded, true);
            k_work_reschedule(&play_work, K_MSEC(TAP_MS));
        } else {
            k_work_reschedule(&play_work, K_NO_WAIT); /* unprintable: skip */
        }
    }
out:
    k_mutex_unlock(&lock);
}

static void start_macro(uint8_t n) {
    k_mutex_lock(&lock, K_FOREVER);
    if (playing) {
        goto out; /* one at a time */
    }
    uint16_t p = 0;

    for (uint8_t i = 0; i < n; i++) { /* skip to macro n */
        while (p < sizeof(buf) && buf[p]) {
            /* a delay's digits and keycodes can never be 0, so every 0 is a terminator */
            p++;
        }
        p++;
    }
    if (p >= sizeof(buf) || buf[p] == 0) {
        goto out; /* empty */
    }
    pos = p;
    playing = true;
    k_work_reschedule(&play_work, K_NO_WAIT);
out:
    k_mutex_unlock(&lock);
}

/* ---- buffer access (VIA) ------------------------------------------------------ */

void via_macro_read(uint16_t offset, uint8_t len, uint8_t *out) {
    k_mutex_lock(&lock, K_FOREVER);
    for (unsigned i = 0; i < len; i++) {
        out[i] = offset + i < sizeof(buf) ? buf[offset + i] : 0;
    }
    k_mutex_unlock(&lock);
}

void via_macro_write(uint16_t offset, uint8_t len, const uint8_t *in) {
    k_mutex_lock(&lock, K_FOREVER);
    stop_playback();
    for (unsigned i = 0; i < len && offset + i < sizeof(buf) - 1; i++) {
        buf[offset + i] = in[i];
    }
    k_work_reschedule(&save_work, SAVE_DELAY);
    k_mutex_unlock(&lock);
}

void via_macro_reset(void) {
    k_mutex_lock(&lock, K_FOREVER);
    stop_playback();
    memset(buf, 0, sizeof(buf));
    k_work_reschedule(&save_work, SAVE_DELAY);
    k_mutex_unlock(&lock);
}

/* ---- the &vmacro behavior ------------------------------------------------------- */

static int on_pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 >= VIA_MACRO_COUNT) {
        return -EINVAL;
    }
    start_macro(binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api via_macro_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &via_macro_api);
#endif
