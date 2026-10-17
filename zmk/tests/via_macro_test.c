/* Host test for via_macro.c: playback, bounds, held keys and saving. The source file is
 * included so its statics are reachable; Zephyr is replaced by mock/. Run with
 * run_host_tests.sh (built with AddressSanitizer, so an out-of-bounds read fails). */
#include <stdio.h>
#include <string.h>

#include "../src/via/via_macro.c"
#include "mock/mock.h"

#define MAX_EVENTS 512

static struct {
    uint32_t enc;
    bool pressed;
    int64_t t;
} ev[MAX_EVENTS];
static int ev_count;

int raise_zmk_keycode_state_changed_from_encoded(uint32_t encoded, bool pressed, int64_t t) {
    if (ev_count < MAX_EVENTS) {
        ev[ev_count].enc = encoded;
        ev[ev_count].pressed = pressed;
        ev[ev_count].t = t;
        ev_count++;
    }
    return 0;
}

static int failures;

#define CHECK(cond, ...)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL line %d: ", __LINE__);                                                    \
            printf(__VA_ARGS__);                                                                   \
            printf("\n");                                                                          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void fresh(void) {
    mock_reset_clock();
    mock_settings_reset();
    memset(buf, 0, sizeof(buf));
    playing = false;
    tap_pending = false;
    held_count = 0;
    pos = 0;
    ev_count = 0;
}

/* Put raw macro bytes at the start of the buffer and play macro n to the end. */
static void load(const char *bytes, size_t len) {
    memcpy(buf, bytes, len);
}

static void play(int n) {
    struct zmk_behavior_binding b = {n, 0};
    struct zmk_behavior_binding_event e = {0};

    CHECK(mock_behavior_api->binding_pressed(&b, e) == 0, "press failed");
    mock_advance(60000);
}

static int downs(void) {
    int n = 0;

    for (int i = 0; i < ev_count; i++) {
        n += ev[i].pressed;
    }
    return n;
}

static int net_down(void) { /* keys still held at the end */
    int n = 0;

    for (int i = 0; i < ev_count; i++) {
        n += ev[i].pressed ? 1 : -1;
    }
    return n;
}

static void test_text(void) {
    fresh();
    load("hi\0", 3);
    play(0);
    CHECK(ev_count == 4, "hi: %d events", ev_count);
    CHECK(ev[0].enc == ((7u << 16) | 0x0B) && ev[0].pressed, "h down");
    CHECK(ev[1].enc == ev[0].enc && !ev[1].pressed, "h up");
    CHECK(ev[2].enc == ((7u << 16) | 0x0C) && ev[2].pressed, "i down");
    CHECK(net_down() == 0 && !playing, "left a key down or still playing");
}

static void test_shifted_char(void) {
    fresh();
    load("A\0", 2);
    play(0);
    CHECK(ev_count == 2 && ev[0].enc == ((7u << 16) | 0x04 | (0x02u << 24)), "A is shift + a");
}

static void test_second_macro(void) {
    fresh();
    load("ab\0cd\0", 6);
    play(1);
    CHECK(ev_count == 4 && ev[0].enc == ((7u << 16) | 0x06), "macro 1 starts with c, got 0x%x",
          ev_count ? ev[0].enc : 0);
}

static void test_empty_macro(void) {
    fresh();
    load("ab\0", 3);
    play(1);
    CHECK(ev_count == 0 && !playing, "an empty macro does nothing");
}

static void test_tap_and_down_up(void) {
    fresh();
    load("\x01\x02\xE1\x01\x01\x04\x01\x03\xE1\0", 10); /* shift down, tap a, shift up */
    play(0);
    CHECK(ev_count == 4, "%d events", ev_count);
    CHECK(ev[0].pressed && ev[0].enc == ((7u << 16) | 0xE1), "shift down first");
    CHECK(ev[3].enc == ((7u << 16) | 0xE1) && !ev[3].pressed, "shift up last");
    CHECK(net_down() == 0, "nothing stuck");
}

static void test_delay(void) {
    fresh();
    load("\x01\x04" "100|a\0", 9);
    play(0);
    CHECK(ev_count == 2 && ev[0].t >= 100, "key before the 100 ms delay ended (t=%lld)",
          (long long)ev[0].t);
}

static void test_keycode_zero(void) {
    fresh();
    load("\x01\x01\0", 3);
    memcpy(buf + 3, "ab\0", 3); /* a second macro right behind */
    play(0);
    CHECK(ev_count == 0 && !playing, "keycode 0 must stop, not press");
    ev_count = 0;
    play(1);
    CHECK(downs() == 2, "the next macro still plays (%d)", downs());
}

static void test_truncated_pair_at_end(void) {
    fresh();
    buf[1021] = 0x01; /* action pair whose keycode byte would be the terminator */
    buf[1022] = 0x01;
    playing = true;
    pos = 1021;
    k_work_reschedule(&play_work, K_NO_WAIT);
    mock_advance(1000);
    CHECK(ev_count == 0 && !playing, "no keycode may be read past the end (%d events)", ev_count);
}

static void test_pos_past_end(void) {
    fresh();
    playing = true;
    pos = sizeof(buf);
    k_work_reschedule(&play_work, K_NO_WAIT);
    mock_advance(1000);
    CHECK(!playing, "playback must stop at the end of the buffer");
}

static void test_held_overflow(void) {
    fresh();
    uint8_t m[40], n = 0;

    for (uint8_t k = 0; k < MAX_HELD + 2; k++) { /* two more downs than the table holds */
        m[n++] = 0x01;
        m[n++] = 0x02;
        m[n++] = 0x04 + k;
    }
    m[n++] = 0;
    load((const char *)m, n);
    play(0);
    CHECK(downs() == MAX_HELD + 1 - 1, "pressed %d keys, table holds %d", downs(), MAX_HELD);
    CHECK(net_down() == 0, "every pressed key was released (%d left)", net_down());
}

static void test_write_stops_and_releases(void) {
    fresh();
    load("\x01\x02\xE1" "abcdefghij\0", 14);
    struct zmk_behavior_binding b = {0, 0};
    struct zmk_behavior_binding_event e = {0};

    mock_behavior_api->binding_pressed(&b, e);
    mock_advance(100); /* shift is down, typing has begun */
    CHECK(net_down() >= 1, "shift should be held now");
    via_macro_write(100, 1, (const uint8_t *)"x");
    CHECK(net_down() == 0 && !playing, "a write must stop the macro and release shift");
}

static void test_write_protects_terminator(void) {
    fresh();
    uint8_t ff[8];

    memset(ff, 0xFF, sizeof(ff));
    via_macro_write(sizeof(buf) - 4, sizeof(ff), ff);
    CHECK(buf[sizeof(buf) - 1] == 0, "last byte must stay 0");
    uint8_t out[4];

    via_macro_read(sizeof(buf) - 2, 4, out);
    CHECK(out[2] == 0 && out[3] == 0, "reads past the end are zero");
}

static void save_hook(const char *name, const void *value, size_t len) {
    CHECK(lock.depth == 0, "flash write while holding the macro lock");
    CHECK(strcmp(name, "via_macro/buf") == 0 && len == VIA_MACRO_SIZE, "save target");
    CHECK(memcmp(value, buf, len) == 0, "saved data is the buffer");
}

static void test_save(void) {
    fresh();
    mock_on_save = save_hook;
    via_macro_write(0, 3, (const uint8_t *)"hi");
    mock_advance(4000);
    CHECK(mock_saved_count == 0, "saved before the 5 s delay");
    mock_advance(2000);
    CHECK(mock_saved_count == 1, "saved %d times", mock_saved_count);
    mock_save_error = -5;
    via_macro_reset();
    mock_advance(6000); /* a failing save must not crash or loop */
    CHECK(mock_saved_count == 2, "save retried?");
}

static ssize_t fill_ff(void *cb_arg, void *data, size_t len) {
    ARG_UNUSED(cb_arg);
    memset(data, 0xFF, len);
    return len;
}

static void test_settings_load(void) {
    fresh();
    CHECK(settings_set("other", 4, fill_ff, NULL) == -ENOENT, "unknown key");
    CHECK(settings_set("buf", sizeof(buf) + 1, fill_ff, NULL) == -ENOENT, "oversized value");
    CHECK(settings_set("buf", sizeof(buf), fill_ff, NULL) == 0, "load failed");
    CHECK(buf[0] == 0xFF && buf[sizeof(buf) - 1] == 0, "a loaded buffer always ends in 0");
}

static void test_param_range(void) {
    fresh();
    struct zmk_behavior_binding b = {VIA_MACRO_COUNT, 0};
    struct zmk_behavior_binding_event e = {0};

    CHECK(mock_behavior_api->binding_pressed(&b, e) == -EINVAL, "macro 16 does not exist");
}

int main(void) {
    test_text();
    test_shifted_char();
    test_second_macro();
    test_empty_macro();
    test_tap_and_down_up();
    test_delay();
    test_keycode_zero();
    test_truncated_pair_at_end();
    test_pos_past_end();
    test_held_overflow();
    test_write_stops_and_releases();
    test_write_protects_terminator();
    test_save();
    test_settings_load();
    test_param_range();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("via_macro: all checks passed\n");
    return 0;
}
