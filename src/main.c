/**
 * @brief MIDI 1.0 implementation on serial UART for Zephyr
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 20260107
 *
 * license SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>

#include <lvgl.h>
#include <string.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(midi1_serial_monitor, CONFIG_LOG_DEFAULT_LEVEL);

/* Moved to ../drivers */
#include "midi1_serial.h"

/* Some helpers to print out the note name */
#include "note.h"


/**
 * Main thread - this may actually terminate normally (code 0) in zephyr.
 * and the rest of threads keeps running just fine.
 */
int main(void)
{
	const struct device *midi = DEVICE_DT_GET(DT_NODELABEL(midi0));
	if (!device_is_ready(midi)) {
		printk("Serial MIDI1 device not ready\n");
		return 0;
	}
	/* You can either use the pointer to the API or the public interface */
	const struct midi1_serial_api *mid = midi->api;

	/*
	 * Don't start the display straight after powerup (needs some time
	 * to settle
	 */
	k_sleep(K_MSEC(1000));
	
	/* Using the API pointer */
	mid->note_on(midi, CH4, 1, 60);
	k_sleep(K_MSEC(290));

	/* Using the public interface for the driver same effect */
	midi1_serial_note_off(midi, CH4, 1, 60);
	k_sleep(K_MSEC(290));

	while (1) {
		/* Running status is used < 300 ms */
		for (uint8_t value = 0; value < 16; value++) {
			/* CC1 sweep */
			//midi1_serial_control_change(midi, CH16, 1, value);
			mid->control_change(midi, CH16, 1, value);
			k_sleep(K_MSEC(290));
		}
		/* Running status is not used > 300 ms */
		for (uint8_t value = 0; value < 16; value++) {
			/* note sweep */
			//midi1_serial_note_on(midi, CH7, value, 100);
			mid->note_on(midi, CH7, value, 100);
			k_sleep(K_MSEC(310));
		}
		/* Send as quickly as the uart poll out will allow */
		for (uint8_t value = 0; value < 16; value++) {
			/* note off sweep */
			//midi1_serial_note_off(midi, CH7, value, 100);
			mid->note_off(midi, CH7, value, 100);
		}
		for (uint8_t value = 0; value < 16; value++) {
			// midi1_serial_start(midi);
			mid->start(midi);
			k_sleep(K_MSEC(100));
			for (uint8_t i = 0; i < 128; i++) {
				// midi1_serial_timingclock(midi);
				mid->timingclock(midi);
				k_sleep(K_MSEC(1));
			}
			// midi1_serial_stop(midi);
			mid->stop(midi);
			k_sleep(K_MSEC(100));
		}
	}
	return 0;
}



/* EOF */
