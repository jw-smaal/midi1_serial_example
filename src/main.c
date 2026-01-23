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

/* Moved to ../drivers */
//#include "midi1.h"
#include "midi1_serial.h"

/**
 * @brief Callbacks/delegates for 'midi1_serial.c' after parsing MIDI1.0
 *
 * @note
 * Do not block in these function as they are called from the MIDI
 * parser this one is blocked untill the delegate is finished.
 * It's better if you have something running for a longer time
 * to fire off a workqueue. 
 */
void note_on_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	printk("Note  on: %03d %03d\n", note, velocity);
}

void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	printk("Note off: %03d %03d\n", note, velocity);
}

void pitchwheel_handler(uint8_t channel, uint8_t lsb, uint8_t msb)
{
	/* 14 bit value for the pitch wheel  */
	int16_t pwheel = (int16_t) ((msb << 7) | lsb) - PITCHWHEEL_CENTER;
	/* print on the serial out */
	printk("Pitchwheel: %d\n", pwheel);
}

void control_change_handler_model(uint8_t channel,
                                  uint8_t controller, uint8_t value)
{
	printk("CC: %d %d\n", controller, value);
}

void control_change_handler(uint8_t channel, uint8_t controller, uint8_t value)
{
	printk("CC: %d %d\n", controller, value);
}

void realtime_handler(uint8_t msg)
{
	printk("Realtime: %d\n", msg);
}

void sysex_start_handler(void)
{
	printk("sysex_start_handler()\n");
}

void sysex_data_handler(uint8_t data)
{
	printk("%x ", data);
}

void sysex_stop_handler(void)
{
	printk("\nsysex_stop_handler()\n");
}

/* ---------------------------- THREADS ------------------------------------ */

/*
 * MIDI1.0 5PIN DIN serial receive parser thread.
 */
void midi1_serial_receive_thread(void)
{
	while(1){
	
	}
}
//K_THREAD_DEFINE(midi1_serial_receive_tid, 512,
//               midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0, 0);


/**
 * Main thread - this may actually terminate normally (code 0) in zephyr.
 * and the rest of threads keeps running just fine.
 */
int main(void) {
	const struct device *midi = DEVICE_DT_GET(DT_NODELABEL(midi0));

        if (!device_is_ready(midi)) {
                printk("MIDI1-Serial device not ready\n");
                return 0;
        }

        while (1) {
		/* Running status is used < 300 ms */ 
		for (uint8_t value = 0; value < 16; value++) {
			midi1_serial_control_change(midi, CH16, 1, value);   /* CC1 sweep */
			k_sleep(K_MSEC(290));
		}
		/* Running status is not used > 300 ms */ 
		for (uint8_t value = 0; value < 16; value++) {
			midi1_serial_note_on(midi, CH7, value, 100);   /* note sweep */
			k_sleep(K_MSEC(310));
		}
		/* Send as quickly as the uart poll out will allow */ 
		for (uint8_t value = 0; value < 16; value++) {
			midi1_serial_note_off(midi, CH7, value, 100);   /* note off sweep */
		}
		for (uint8_t value = 0; value < 16; value++) {
			midi1_serial_start(midi);
			k_sleep(K_MSEC(100));
			for (uint8_t i =0; i < 128; i++) {
				midi1_serial_timingclock(midi);
				k_sleep(K_MSEC(1));
			}
			midi1_serial_stop(midi);
			k_sleep(K_MSEC(100));
		}
        }

        return 0;
}
/* EOF */
