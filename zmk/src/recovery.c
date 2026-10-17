/*
 * Fn+Esc: enter MCUboot's serial recovery by invalidating the app image.
 *
 * The retained-RAM boot-mode flag that Zephyr's &bootloader uses does not survive this
 * chip's reset, so MCUboot never sees it. Erasing the slot 0 header sector is what
 * scripts/force_recovery.py already does over Bluetooth, and it makes MCUboot enter
 * recovery on its own (CONFIG_BOOT_SERIAL_NO_APPLICATION).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <crush80_recovery.h>

LOG_MODULE_REGISTER(recovery, CONFIG_LOG_DEFAULT_LEVEL);

#define SLOT0_HEADER_OFFSET 0x10000 /* same as SLOT0_START in flash_mgmt.c */
#define FLASH_SECTOR_SIZE   4096

void crush80_enter_recovery(void) {
    const struct device *flash = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));

    if (!device_is_ready(flash)) {
        LOG_ERR("flash not ready, staying in the app");
        return;
    }

    int err = flash_erase(flash, SLOT0_HEADER_OFFSET, FLASH_SECTOR_SIZE);

    if (err) {
        LOG_ERR("could not erase the image header (%d), staying in the app", err);
        return;
    }

    LOG_INF("image header erased, rebooting into recovery");
    k_msleep(50); /* let the log flush */
    sys_reboot(SYS_REBOOT_COLD);
}
