#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/init.h>
#include <string.h>
#include "zmk_adapter.h"
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>   /* ZMK_KEYMAP_LEN */
#include <zmk/behavior.h> /* struct zmk_behavior_binding */
#include <zmk/hid_indicators.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/bt.h>
#include <dt-bindings/zmk/outputs.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include "engine.h"
#include "overlay.h"

LOG_MODULE_REGISTER(rrgb_adapter, CONFIG_LOG_DEFAULT_LEVEL);

#define STRIP_NODE  DT_CHOSEN(zmk_underglow)
#define STRIP_N     DT_PROP(STRIP_NODE, chain_length)

static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb hw[STRIP_N];

int rrgb_strip_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("led_strip not ready");
        return -1;
    }
    if (led_strip_length(strip) < STRIP_N) {
        LOG_ERR("strip too short: %zu < %d", led_strip_length(strip), STRIP_N);
        return -1;
    }
    return 0;
}

/* Layers are found by display name, not number: the numbers depend on the keymap
 * (and on live edits made in ZMK Studio). */
static int layer_id_by_name(const char *want) {
    for (int id = 0; id < ZMK_KEYMAP_LAYERS_LEN; id++) {
        const char *name = zmk_keymap_layer_name(id);

        if (name && strcmp(name, want) == 0) {
            return id;
        }
    }
    return -1;
}

static int fn_layer_id(void) {
    return layer_id_by_name("Fn");
}

/* Colours the Fn overlay shows (the keys light up as what they do, and a few
 * also show what is currently selected). */
static const struct rrgb COL_BOUND = {255, 255, 255};   /* bound key */
static const struct rrgb COL_USB = {0, 255, 60};        /* USB is the output */
static const struct rrgb COL_BT = {0, 90, 255};         /* active Bluetooth profile, connected */
static const struct rrgb COL_BT_WAIT = {255, 140, 0};   /* active profile, waiting for a host */
static const struct rrgb COL_MODE = {255, 70, 0};       /* a mode toggle that is on */

/* Tell the overlay whether Fn is held and, if so, how to colour each key. Read
 * from the live keymap, so it follows VIA and ZMK Studio edits: a key that
 * selects a Bluetooth profile, USB output or Mac mode shows that state wherever
 * it is bound. Anything else bound on Fn (not &trans / &none) is plain white. */
static void fn_overlay_update(void) {
    int fn = fn_layer_id();

    if (fn < 0 || !zmk_keymap_layer_active(fn)) {
        rrgb_overlay_set_fn(false, NULL);
        return;
    }

    int mac = layer_id_by_name("Mac");
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    struct rrgb colors[RRGB_FN_POSITIONS] = {0};

    for (int pos = 0; pos < ZMK_KEYMAP_LEN && pos < RRGB_FN_POSITIONS; pos++) {
        const struct zmk_behavior_binding *b = zmk_keymap_get_layer_binding_at_idx(fn, pos);

        if (!b || !b->behavior_dev || strcmp(b->behavior_dev, "transparent") == 0 ||
            strcmp(b->behavior_dev, "none") == 0) {
            continue;
        }
        struct rrgb c = COL_BOUND;

        if (strcmp(b->behavior_dev, "bluetooth") == 0 && b->param1 == BT_SEL_CMD &&
            ep.transport == ZMK_TRANSPORT_BLE && b->param2 == zmk_ble_active_profile_index()) {
            c = zmk_ble_active_profile_is_connected() ? COL_BT : COL_BT_WAIT;
        } else if (strcmp(b->behavior_dev, "outputs") == 0 && b->param1 == OUT_USB &&
                   ep.transport == ZMK_TRANSPORT_USB) {
            c = COL_USB;
        } else if (strcmp(b->behavior_dev, "toggle_layer") == 0 && mac >= 0 &&
                   (int)b->param1 == mac && zmk_keymap_layer_active(mac)) {
            c = COL_MODE;
        }
        colors[pos] = c;
    }
    rrgb_overlay_set_fn(true, colors);
}

static int rrgb_event_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev) { rrgb_on_key(ev->position, ev->state); }

    const struct zmk_layer_state_changed *lev = as_zmk_layer_state_changed(eh);
    if (lev) { fn_overlay_update(); }

    if (as_zmk_endpoint_changed(eh) || as_zmk_ble_active_profile_changed(eh)) { fn_overlay_update(); }

    const struct zmk_hid_indicators_changed *iev = as_zmk_hid_indicators_changed(eh);
    if (iev) { rrgb_overlay_set_caps((iev->indicators & BIT(1)) != 0); }

    const struct zmk_battery_state_changed *bev = as_zmk_battery_state_changed(eh);
    if (bev) { rrgb_overlay_set_battery(bev->state_of_charge); }

#if IS_ENABLED(CONFIG_RAINY_RGB_IDLE_BLANK)
    const struct zmk_activity_state_changed *aev = as_zmk_activity_state_changed(eh);
    if (aev) { rrgb_set_idle(aev->state != ZMK_ACTIVITY_ACTIVE); }
#endif

    return ZMK_EV_EVENT_BUBBLE;   /* passive observer */
}
ZMK_LISTENER(rrgb_listener, rrgb_event_listener);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_position_state_changed);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(rrgb_listener, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_RAINY_RGB_IDLE_BLANK)
ZMK_SUBSCRIPTION(rrgb_listener, zmk_activity_state_changed);
#endif

static int rrgb_overlay_seed(void) {
    rrgb_overlay_set_caps((zmk_hid_indicators_get_current_profile() & BIT(1)) != 0);
    fn_overlay_update();
    rrgb_overlay_set_battery(zmk_battery_state_of_charge());
    return 0;
}
SYS_INIT(rrgb_overlay_seed, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void rrgb_strip_show(const struct rrgb *px, uint16_t n) {
    if (n > STRIP_N) { n = STRIP_N; }
    for (uint16_t i = 0; i < n; i++) {
        hw[i] = (struct led_rgb){ .r = px[i].r, .g = px[i].g, .b = px[i].b };
    }
    (void)led_strip_update_rgb(strip, hw, n);
}

/* PC2 gates LED VCC through a MOSFET (active-high). The led_strip driver
 * configures the pin and powers the rail at init, poweroff.c drops it for
 * deep sleep; here the render loop cuts it while the strip stays dark —
 * a blanked WS2812 still draws ~0.5-1 mA quiescent, ~40-80 mA for 83 LEDs.
 *
 * This file owns the pin: everything that drives or samples the rail goes
 * through rrgb_strip_power() / rrgb_strip_rail_state() so the register
 * addresses live in exactly one place. */
#define B91_GPIO_PC_OEN  0x80140312UL   /* output enable, 0 = enabled */
#define B91_GPIO_PC_OUT  0x80140313UL   /* output data */
#define B91_GPIO_PC_GPIO 0x80140316UL   /* GPIO mode enable */

void rrgb_strip_power(bool on) {
#if IS_ENABLED(CONFIG_LED_STRIP_B91_SPI_PC2_POWER)
    if (on) {
        /* Re-assert the whole drive configuration, not just the level.
         * Writing the data bit alone only powers the rail if the pin is
         * still a GPIO output; if anything reconfigured PC2 behind our
         * back, the strip stays dark and the write looks like it worked.
         * (The analog input-buffer and pull settings from the driver's
         * init do not gate the MOSFET, so they are not replayed here.) */
        sys_write8(sys_read8(B91_GPIO_PC_GPIO) | BIT(2), B91_GPIO_PC_GPIO);
        sys_write8(sys_read8(B91_GPIO_PC_OEN) & ~BIT(2), B91_GPIO_PC_OEN);
        sys_write8(sys_read8(B91_GPIO_PC_OUT) | BIT(2), B91_GPIO_PC_OUT);
    } else {
        sys_write8(sys_read8(B91_GPIO_PC_OUT) & ~BIT(2), B91_GPIO_PC_OUT);
    }
    LOG_INF("led rail %s", on ? "on" : "off");
#else
    ARG_UNUSED(on);
#endif
}

uint8_t rrgb_strip_rail_state(void) {
#if IS_ENABLED(CONFIG_LED_STRIP_B91_SPI_PC2_POWER)
    uint8_t state = 0;

    if (sys_read8(B91_GPIO_PC_OUT) & BIT(2)) {
        state |= RRGB_RAIL_HIGH;
    }
    if (!(sys_read8(B91_GPIO_PC_OEN) & BIT(2))) {   /* 0 = output enabled */
        state |= RRGB_RAIL_OUT_EN;
    }
    if (sys_read8(B91_GPIO_PC_GPIO) & BIT(2)) {
        state |= RRGB_RAIL_GPIO_MODE;
    }
    return state;
#else
    return 0;
#endif
}
