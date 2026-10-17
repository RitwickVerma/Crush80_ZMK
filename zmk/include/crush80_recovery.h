/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * Reboot into MCUboot's USB serial recovery (Fn+Esc, the &bootloader key).
 *
 * Erases the header sector of slot 0, so MCUboot finds no valid image and stays in
 * recovery, then resets. Returns only if the erase failed (the keyboard keeps running).
 * The keyboard stays in recovery until a new image is flashed (docs/flashing.md).
 */
void crush80_enter_recovery(void);
