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
	printk("Note  on: %s %03d %03d\n", noteToTextWithOctave(note, false),
	       note, velocity);
}

void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	printk("Note off: %s %03d %03d\n", noteToTextWithOctave(note, false),
	       note, velocity);
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

/**
 * Serial receive parser thread - receiveparser keeps reading data
 * filled by the ISR and then calls callbacks.
 */
void midi1_serial_receive_thread(void)
{
	const struct device *midi = DEVICE_DT_GET(DT_NODELABEL(midi0));

	if (!device_is_ready(midi)) {
		printk("receive_thread Serial MIDI1 device not ready\n");
		return;
	}
	const struct midi1_serial_api *mid = midi->api;

	/*
	 * Set the callbacks in the driver to our own callbacks.  Pointers
	 * left null are not used in the callbacks.
	 * e.g. right now the aftertouch is not handled.
	 */
	struct midi1_serial_callbacks my_cb = {
		.note_on = note_on_handler,
		.note_off = note_off_handler,
		.control_change = control_change_handler,
		.pitchwheel = pitchwheel_handler,
		.sysex_start = sysex_start_handler,
		.sysex_data = sysex_data_handler,
		.sysex_stop = sysex_stop_handler
	};
	/* midi1_serial_register_callbacks(midi, &my_cb); */
	mid->register_callbacks(midi, &my_cb);

	while (1) {
		/* As this call is blocking no need to sleep in between */
		mid->receiveparser(midi);
	}
}

K_THREAD_DEFINE(midi1_serial_receive_tid, 4096,
                midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0, 0);


/*
 * LVGL stuff
 */

static lv_obj_t *line1;
static lv_obj_t *line2;
static lv_obj_t *line3;

static lv_obj_t *cursor;

static void initialize_gui(void)
{
	/* Set screen background to black */
	lv_obj_set_style_bg_color(lv_screen_active(),
				  lv_color_black(),
				  LV_PART_MAIN);
	lv_obj_set_style_bg_opa(lv_screen_active(),
				LV_OPA_COVER,
				LV_PART_MAIN);
	
	/* Set default text color + font for the whole screen */
	lv_obj_set_style_text_color(lv_screen_active(),
				    lv_color_hex(0x00ff00),
				    LV_PART_MAIN);
	lv_obj_set_style_text_font(lv_screen_active(),
				   &lv_font_montserrat_24,
				   LV_PART_MAIN);
	
	/* Draw retro terminal frame */
	lv_obj_t *frame = lv_obj_create(lv_screen_active());
	lv_obj_set_size(frame, 340, 280);  /* Adjust to your display */
	lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_border_color(frame, lv_color_hex(0x00aa00), LV_PART_MAIN);
	lv_obj_set_style_border_width(frame, 5, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, LV_PART_MAIN);
	
	const int margin = 8;
	
	/* Line 1 */
	line1 = lv_label_create(frame);
	lv_obj_set_style_text_color(line1,
				    lv_color_hex(0x00ff00),
				    LV_PART_MAIN);
	lv_label_set_text(line1, "MIDI Monitor v0.1 by Jan-Willem Smaal");
	lv_obj_align(line1, LV_ALIGN_TOP_LEFT, margin, margin);
	
	/* Line 2 */
	line2 = lv_label_create(frame);
	lv_obj_set_style_text_color(line2,
				    lv_color_hex(0x0000ff),
				    LV_PART_MAIN);
	lv_label_set_text(line2, "-");
	lv_obj_align(line2, LV_ALIGN_TOP_LEFT, margin, margin + 26);
	
	/* Line 3 */
	line3 = lv_label_create(frame);
	lv_label_set_text(line3, "Waiting...");
	lv_obj_align(line3, LV_ALIGN_TOP_LEFT, margin, margin + 52);
	
	/* Blinking cursor */
	cursor = lv_label_create(frame);
	lv_label_set_text(cursor, "_");
	lv_obj_align(cursor, LV_ALIGN_TOP_LEFT, margin + 160, margin + 52);
	
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, cursor);
	lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
	lv_anim_set_time(&a, 500);
	lv_anim_set_playback_time(&a, 500);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
	lv_anim_start(&a);
}


static void initialize_gui2(void)
{
	/* Set screen background to black */
	lv_obj_set_style_bg_color(lv_screen_active(),
				  lv_color_black(),
				  LV_PART_MAIN);
	lv_obj_set_style_bg_opa(lv_screen_active(),
				LV_OPA_COVER,
				LV_PART_MAIN);
	
	/* Set default text color + font for the whole screen */
	lv_obj_set_style_text_color(lv_screen_active(),
				    lv_color_hex(0x00ff00),
				    LV_PART_MAIN);
	lv_obj_set_style_text_font(lv_screen_active(),
				   &lv_font_montserrat_24,
				   LV_PART_MAIN);
	
	/* Padding from edges */
	const int margin = 6;
	
	/* Line 1 */
	line1 = lv_label_create(lv_screen_active());
	lv_label_set_text(line1, "MIDI Monitor v0.1 by Jan-Willem Smaal");
	lv_obj_align(line1, LV_ALIGN_TOP_LEFT, margin, margin);
	
	/* Line 2 */
	line2 = lv_label_create(lv_screen_active());
	lv_label_set_text(line2, "-");
	lv_obj_align(line2, LV_ALIGN_TOP_LEFT, margin, margin + 26);
	
	/* Line 3 */
	line3 = lv_label_create(lv_screen_active());
	lv_label_set_text(line3, "Waiting...");
	lv_obj_align(line3, LV_ALIGN_TOP_LEFT, margin, margin + 52);
}


void lvgl_thread(void)
{
	const struct device *display_dev;
	int ret;
	
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}
	
	initialize_gui();
	
	lv_timer_handler();
	ret = display_blanking_off(display_dev);
	if (ret < 0 && ret != -ENOSYS) {
		LOG_ERR("Failed to turn blanking off (error %d)", ret);
		return;
	}
	
	while (1) {
		//lv_label_set_text(line2, "Control Change 001 = 112");
		//lv_label_set_text(line3, "Channel 13");
		
		uint32_t sleep_ms = lv_timer_handler();
		
		k_msleep(MIN(sleep_ms, INT32_MAX));
	}
	
	return;
}

K_THREAD_DEFINE(lvgl_thread_tid, 4096,
		lvgl_thread, NULL, NULL, NULL, 5, 0, 0);



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
