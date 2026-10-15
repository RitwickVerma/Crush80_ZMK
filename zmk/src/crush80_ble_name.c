/*
 * Per-profile BLE advertising name for Crush80.
 *
 * Updates the BLE advertising name to "Crush 80 N" (where N = profile + 1)
 * whenever the active BLE profile changes. This makes each profile appear with
 * a distinct name in the host's Bluetooth device list.
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BASE_NAME CONFIG_ZMK_KEYBOARD_NAME

/* restart_adv: re-advertise under the new name. ZMK restarts advertising
 * before it raises the profile-changed event, so without this the scan list
 * keeps showing the previous profile's name. Not at boot: BLE is not up yet. */
static int update_ble_name(uint8_t profile_index, bool restart_adv) {
    char name[CONFIG_BT_DEVICE_NAME_MAX + 1];

    snprintf(name, sizeof(name), "%s %d", BASE_NAME, profile_index + 1);

    int err = restart_adv ? zmk_ble_set_device_name(name) : bt_set_name(name);
    if (err) {
        LOG_ERR("Failed to set BLE name to '%s' (err %d)", name, err);
        return err;
    }

    LOG_DBG("BLE name updated: %s", name);
    return 0;
}

static int on_profile_changed(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev) {
        update_ble_name(ev->index, true);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(crush80_ble_name, on_profile_changed);
ZMK_SUBSCRIPTION(crush80_ble_name, zmk_ble_active_profile_changed);

static int crush80_ble_name_init(void) {
    update_ble_name(0, false);
    return 0;
}

SYS_INIT(crush80_ble_name_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
