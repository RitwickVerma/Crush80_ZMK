/*
 * Fn+Esc: enter MCUboot's serial recovery by invalidating the app image.
 *
 * The retained-RAM boot-mode flag that Zephyr's &bootloader uses does not survive this
 * chip's reset, so MCUboot never sees it. Erasing the slot 0 header sector is what
 * scripts/force_recovery.py already does over Bluetooth, and it makes MCUboot enter
 * recovery on its own (CONFIG_BOOT_SERIAL_NO_APPLICATION).
 *
 * That sector also holds code the running firmware executes from flash, so after the
 * erase nothing may run from flash, not even sys_reboot(): the erase and the reset
 * both happen from RAM with interrupts off (same approach as the commit trampoline in
 * flash_mgmt.c).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <crush80_recovery.h>

LOG_MODULE_REGISTER(recovery, CONFIG_LOG_DEFAULT_LEVEL);

#define SLOT0_HEADER_OFFSET 0x10000 /* same as SLOT0_START in flash_mgmt.c */

/* B91 HAL functions that live in .ram_code (the only ones safe once flash is erased) */
extern void flash_erase_sector_ram(unsigned long addr);
extern void flash_unlock_ram(unsigned long mid_type);

#define REG_TMR_CTRL2 (*(volatile unsigned char *)(0x80140140 + 0x02)) /* watchdog enable */
#define TMR_WD_EN_BIT 0x80
#define REG_RESET (*(volatile unsigned char *)0x801401ef) /* write BIT(5): reset the MCU */

__attribute__((section(".ram_code"))) __attribute__((noinline)) static void
erase_header_and_reset(void) {
    __asm__ volatile("csrw 0x304, zero");                  /* MIE = 0 */
    __asm__ volatile("csrc 0x300, %0" ::"r"(1UL << 3)); /* MSTATUS.MIE = 0 */

    REG_TMR_CTRL2 &= ~TMR_WD_EN_BIT; /* nothing can feed the watchdog from here on */

    /* The erase routine does not disable the branch target buffer itself; with it on, the
     * CPU speculatively fetches from flash while the bus is in manual SPI mode. */
    __asm__ volatile("csrci 0x7D0, 8");
    __asm__ volatile("fence.i");

    flash_unlock_ram(0); /* clear write protection */
    flash_erase_sector_ram(SLOT0_HEADER_OFFSET);

    REG_RESET = 0x20;
    while (1) {
    }
}

void crush80_enter_recovery(void) {
    LOG_INF("erasing the image header and rebooting into recovery");
    k_msleep(50); /* let the log flush */
    erase_header_and_reset();
}
