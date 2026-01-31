/**
 * LVGL stuff
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

#include "common.h"

LOG_MODULE_REGISTER(lvgl_screen1, CONFIG_LOG_DEFAULT_LEVEL);
/*
 *  GLOBALS
 */
static lv_obj_t *label_title;
static lv_obj_t *label_bpm;
static lv_obj_t *label_channel;
static lv_obj_t *ta_midi;

/*
 *  GUI INITIALIZATION (480×320 landscape)
 */
static void initialize_gui(void)
{
	/* Screen background (solid black, no transparency) */
	lv_obj_set_style_bg_color(lv_screen_active(),
	                          lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

	/* Default text: white, size 14 */
	lv_obj_set_style_text_color(lv_screen_active(),
	                            lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lv_screen_active(),
	                           &lv_font_montserrat_14, LV_PART_MAIN);

	/* =====================================================
	 *  TOP BAR
	 * ===================================================== */
	/* Left: static title */
	label_title = lv_label_create(lv_screen_active());
	lv_label_set_text(label_title, "MIDI Monitor by J-W Smaal v0.1");
	lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 6, 4);

	/* Right: BPM in larger font */
	label_bpm = lv_label_create(lv_screen_active());
	lv_obj_set_style_text_font(label_bpm,
	                           &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_style_text_color(label_bpm,
				    lv_color_hex(0xff0000),   /* Red */
				    LV_PART_MAIN);
	lv_label_set_text(label_bpm, " 123.89 BPM");
	lv_obj_align(label_bpm, LV_ALIGN_TOP_RIGHT, 0, 0);
	
	
	/* Left: Channel on the left */
	label_channel = lv_label_create(lv_screen_active());
	lv_obj_set_style_text_font(label_channel,
				   &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_style_text_color(label_channel,
				    lv_color_hex(0x006400),   /* dark green */
				    LV_PART_MAIN);
	lv_label_set_text(label_channel, "CHxx");
	lv_obj_align(label_channel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

	/* =====================================================
	 *  CENTER: LARGE SCROLLABLE TEXT WINDOW
	 * ===================================================== */
	ta_midi = lv_textarea_create(lv_screen_active());
	//lv_label_set_recolor(ta_midi, true);
	lv_obj_set_style_text_font(ta_midi,
				   &lv_font_montserrat_18, LV_PART_MAIN);

	/* Size: nearly full screen minus top and bottom bar */
	lv_obj_set_size(ta_midi, 420, 220);
	lv_obj_align(ta_midi, LV_ALIGN_TOP_LEFT, 6, 40);

	/* No transparency */
	lv_obj_set_style_bg_color(ta_midi, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ta_midi, LV_OPA_COVER, LV_PART_MAIN);

	/* Whitetext by default */
	lv_obj_set_style_text_color(ta_midi, lv_color_white(), LV_PART_MAIN);

	lv_textarea_set_text(ta_midi, "");
	lv_textarea_set_max_length(ta_midi, 4096);
	lv_textarea_set_cursor_click_pos(ta_midi, false);
}

static void ui_set_line(const char *msg)
{
	if (!ta_midi) {
		return;
	}

	char buf[MIDI_LINE_MAX];
	snprintf(buf, sizeof(buf), "%s\n", msg);
	lv_textarea_set_text(ta_midi, buf);
}

#define MAX_MIDI_LINES 9
static int midi_line_count = 0;
static void ui_add_line(const char *msg)
{
	if (!ta_midi) {
		return;
	}
	
	/* If full, clear everything */
	if (midi_line_count >= MAX_MIDI_LINES) {
		lv_textarea_set_text(ta_midi, "");
		midi_line_count = 0;
	}
	
	/* Format new line */
	char buf[MIDI_LINE_MAX];
	snprintf(buf, sizeof(buf), "%s\n", msg);
	
	/* Append */
	lv_textarea_add_text(ta_midi, buf);
	midi_line_count++;
}

#define MAX_MESSAGES_PER_TICK 3
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
	ui_add_line("...");
	initialize_gui();
	
	char line[MIDI_LINE_MAX];
	while (1) {
		int processed = 0;
		
		/* Process at most N messages per iteration */
		while (processed < MAX_MESSAGES_PER_TICK &&
		       k_msgq_get(&midi_msgq, line, K_NO_WAIT) == 0) {
			
			ui_add_line(line);
			processed++;
		}
		
		/* Always give LVGL time to run */
		uint32_t sleep_ms = lv_timer_handler();
		if (sleep_ms < 5) {
			sleep_ms = 5;
		}
		k_msleep(sleep_ms);
	}
	return;
}


/* For LVGL had to increase the stack size */
K_THREAD_DEFINE(lvgl_thread_tid, 4096, lvgl_thread, NULL, NULL, NULL, 5, 0, 0);

/* EOF */
