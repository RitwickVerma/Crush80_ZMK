/*
 * Crush80 RGB TEST — WS2812/SK6812 LED strip (PB7 PSPI) colour sweep
 *
 * The LED strip driver (telink,b91-spi-led-strip) powers PC2 and configures
 * PB7/PSPI in its own init. This test just cycles solid colours through the
 * Zephyr led_strip API so the chain can be visually verified.
 *
 * Typing is unaffected (PB7/PC2 are not matrix pins).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "crush80_rgb.h"

LOG_MODULE_REGISTER(crush80_rgb, LOG_LEVEL_INF);

#define STRIP_NODE      DT_NODELABEL(led_strip)
#define STRIP_DEV       DEVICE_DT_GET(STRIP_NODE)

static struct led_rgb pixels[CRUSH80_LED_COUNT_TOTAL];

void crush80_rgb_set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
	if (index >= CRUSH80_LED_COUNT_TOTAL) {
		return;
	}
	pixels[index].r = r;
	pixels[index].g = g;
	pixels[index].b = b;
}

void crush80_rgb_set_all(uint8_t r, uint8_t g, uint8_t b)
{
	for (uint8_t i = 0; i < CRUSH80_LED_COUNT_TOTAL; i++) {
		pixels[i].r = r;
		pixels[i].g = g;
		pixels[i].b = b;
	}
}

void crush80_rgb_toggle(void) {}
bool crush80_rgb_is_on(void) { return true; }

static void rgb_test_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
	const struct device *strip = STRIP_DEV;

	if (!device_is_ready(strip)) {
		LOG_ERR("led_strip device not ready");
		return;
	}

	LOG_INF("WS2812 test: %d LEDs, cycling colours every 2s",
		CRUSH80_LED_COUNT_TOTAL);

	/* Sweep: white, red, green, blue, off — one full cycle confirms the
	 * whole chain is wired and powered. */
	static const uint8_t cols[][3] = {
		{ 255, 255, 255 }, /* white */
		{ 255,   0,   0 }, /* red */
		{   0, 255,   0 }, /* green */
		{   0,   0, 255 }, /* blue */
		{   0,   0,   0 }, /* off */
	};
	int ci = 0;

	while (1) {
		crush80_rgb_set_all(cols[ci][0], cols[ci][1], cols[ci][2]);
		led_strip_update_rgb(strip, pixels, CRUSH80_LED_COUNT_TOTAL);
		LOG_INF("colour step %d: r=%d g=%d b=%d",
			ci, cols[ci][0], cols[ci][1], cols[ci][2]);
		ci = (ci + 1) % (int)(sizeof(cols) / sizeof(cols[0]));
		k_sleep(K_SECONDS(2));
	}
}

#define RGB_STACK_SIZE 2048
#define RGB_THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(rgb_stack, RGB_STACK_SIZE);
static struct k_thread rgb_thread_data;

static int crush80_rgb_sys_init(void)
{
	k_thread_create(&rgb_thread_data, rgb_stack,
			K_THREAD_STACK_SIZEOF(rgb_stack),
			rgb_test_thread, NULL, NULL, NULL,
			RGB_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&rgb_thread_data, "rgb_test");
	LOG_INF("crush80_rgb: WS2812 colour-sweep test started");
	return 0;
}

SYS_INIT(crush80_rgb_sys_init, APPLICATION, 99);
