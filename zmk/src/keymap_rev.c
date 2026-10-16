/*
 * Layout revision: live edits (VIA, ZMK Studio) are saved per layer number and
 * key position, so when the layer list changes they would land on whatever layer
 * now has that number. Bump CONFIG_CRUSH80_KEYMAP_REV when the layers are
 * redesigned; the first boot of that firmware deletes every saved keymap edit
 * (bindings, layer names, layer order) and falls back to crush80.keymap.
 *
 * Normal updates keep the revision, so they keep your edits. Macros, Bluetooth
 * pairings and RGB settings are stored elsewhere and are not touched.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <zmk/keymap.h>

LOG_MODULE_REGISTER(keymap_rev, CONFIG_LOG_DEFAULT_LEVEL);

#define REV_KEY "crush80/keymap_rev"
#define MAX_LAYERS 32 /* old layouts may have had more layers than this one */
#define MAX_POS 128

static int stored_rev = -1;

static int rev_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "keymap_rev") == 0 && len == sizeof(stored_rev)) {
        read_cb(cb_arg, &stored_rev, sizeof(stored_rev));
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(crush80, "crush80", NULL, rev_set, NULL, NULL);

/* What the keymap settings subtree currently holds. */
static struct {
    uint8_t bind[MAX_LAYERS][MAX_POS / 8];
    uint32_t names;
    bool order;
} found;

static int collect(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param) {
    const char *next;

    if (settings_name_steq(key, "l", &next) && next) {
        char *end;
        unsigned long layer = strtoul(next, &end, 10);

        if (*end == '/') {
            unsigned long pos = strtoul(end + 1, &end, 10);

            if (layer < MAX_LAYERS && pos < MAX_POS) {
                found.bind[layer][pos / 8] |= BIT(pos % 8);
            }
        }
    } else if (settings_name_steq(key, "l_n", &next) && next) {
        unsigned long layer = strtoul(next, NULL, 10);

        if (layer < MAX_LAYERS) {
            found.names |= BIT(layer);
        }
    } else if (strcmp(key, "layer_order") == 0) {
        found.order = true;
    }
    return 0;
}

static void migrate_fn(struct k_work *work) {
    ARG_UNUSED(work);

    int rev = CONFIG_CRUSH80_KEYMAP_REV;

    if (stored_rev == rev) {
        return;
    }

    memset(&found, 0, sizeof(found));
    settings_load_subtree_direct("keymap", collect, NULL);

    unsigned deleted = 0;
    char name[24];

    for (unsigned l = 0; l < MAX_LAYERS; l++) {
        for (unsigned p = 0; p < MAX_POS; p++) {
            if (found.bind[l][p / 8] & BIT(p % 8)) {
                snprintf(name, sizeof(name), "keymap/l/%u/%u", l, p);
                deleted += settings_delete(name) == 0;
            }
        }
        if (found.names & BIT(l)) {
            snprintf(name, sizeof(name), "keymap/l_n/%u", l);
            deleted += settings_delete(name) == 0;
        }
    }
    if (found.order) {
        deleted += settings_delete("keymap/layer_order") == 0;
    }

    zmk_keymap_discard_changes(); /* reload crush80.keymap into RAM */

    if (settings_save_one(REV_KEY, &rev, sizeof(rev)) == 0) {
        LOG_INF("keymap revision %d (was %d): cleared %u saved edits", rev, stored_rev, deleted);
    } else {
        LOG_ERR("could not save the keymap revision");
    }
}

static K_WORK_DELAYABLE_DEFINE(migrate_work, migrate_fn);

/* settings_load() runs from main(), after this; wait for it. */
static int keymap_rev_init(void) {
    k_work_schedule(&migrate_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(keymap_rev_init, APPLICATION, 99);
