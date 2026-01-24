/**
 * @file test_midi1_parser.c
 * @brief MIDI1.0 parsing test cases.
 *
 * @author Jan-Willem Smaal
 * @date 20260118
 * license SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <stdint.h>

#include "midi1.h"
#include "note.h"
#include "midi1_serial.h"

/* Globals to capture callback results */
static uint8_t g_last_note;
static uint8_t g_last_vel;
static uint8_t g_last_ctrl;
static uint8_t g_last_val;
static uint8_t g_last_rt;
static uint8_t g_pw_lsb;
static uint8_t g_pw_msb;
static uint8_t g_channel;

/* For the sysex test */
static uint8_t g_sysex_buf[200];
static int g_sysex_len;

/* Test callbacks */
static void test_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	g_channel = channel;
	g_last_note = note;
	g_last_vel = velocity;
}

static void test_note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	g_channel = channel;
	g_last_note = note;
	g_last_vel = velocity;
}

static void test_cc_cb(uint8_t channel, uint8_t ctrl, uint8_t val)
{
	g_channel = channel;
	g_last_ctrl = ctrl;
	g_last_val = val;
}

static void test_pw_cb(uint8_t channel, uint8_t lsb, uint8_t msb)
{
	g_channel = channel;
	g_pw_lsb = lsb;
	g_pw_msb = msb;
}

/* Realtime has no channel */
static void test_rt_cb(uint8_t msg)
{
	g_last_rt = msg;
}

static void test_sysex_start_cb(void)
{
	printf("test_sysex_start_cb()\n");
	g_sysex_len = 0;
}

static void test_sysex_data_cb(uint8_t data)
{
	printf(" %x ", data);
	g_sysex_buf[g_sysex_len++] = data;
}

static void test_sysex_stop_cb(void)
{
	printf("\n test_sysex_stop_cb()\n");
}

/* Helper: init a fresh instance with callbacks and msgq */
static void midi_test_inst_init(struct midi1_serial_inst *inst)
{
	memset(inst, 0, sizeof(*inst));

	inst->note_on = test_note_on_cb;
	inst->note_off = test_note_off_cb;
	inst->control_change = test_cc_cb;
	inst->realtime = test_rt_cb;
	inst->pitchwheel = test_pw_cb;
	inst->sysex_start = test_sysex_start_cb;
	inst->sysex_data = test_sysex_data_cb;
	inst->sysex_stop = test_sysex_stop_cb;
	
	k_msgq_init(&inst->msgq, inst->msgq_buffer, MSG_SIZE, MSGQ_SIZE);

	midi1_serial_init(inst);

	g_last_note = g_last_vel = 0;
	g_last_ctrl = g_last_val = 0;
	g_last_rt = 0;
	g_pw_lsb = g_pw_msb = 0;
	
}

/* Helper: feed bytes into msgq and run parser */
static void feed_and_parse(struct midi1_serial_inst *inst,
			   const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		k_msgq_put(&inst->msgq, &data[i], K_NO_WAIT);
		midi1_serial_receiveparser(inst);
	}
}

/* Include the leading space for the note!  */
ZTEST(midi1_parser, test_note_to_text)
{
	const char *notetext = noteToText(60, false);
	printf("noteToText(60) -> %s\n", notetext);
	zassert_str_equal(notetext, "C ", "Expect C");
}


ZTEST(midi1_parser, test_note_to_text2)
{
	const char *notetext = noteToText(61, false);
	printf("noteToText(61) -> %s\n", notetext);
	zassert_str_equal(notetext, "C#", "Expect C");
}

ZTEST(midi1_parser, test_note_to_freq)
{
	float freq = noteToFreq(69);
	zassert_equal(freq, 440.0000000000f, "Expected 440 Hz");
}

ZTEST(midi1_parser, test_freq_to_note)
{
	uint8_t note = freqToMidiNote(440.0000000000f);
	printf("freqToMidiNote(440.0) -> %d\n", note);
	zassert_equal(note, 69, "Expected note 69");
}

ZTEST(midi1_parser, test_freq_to_note2)
{
	uint8_t note = freqToMidiNote(220.0000000000f);
	printf("freqToMidiNote(440.0) -> %d\n", note);
	zassert_equal(note, 57, "Expected note 57");
}

ZTEST(midi1_parser, test_freq_to_note3)
{
	uint8_t note = freqToMidiNote(1661.2f);
	printf("freqToMidiNote(1661.2f) -> %d\n", note);
	zassert_equal(note, 92, "Expected note 92");
}

/* Basic NOTE ON test */
ZTEST(midi1_parser, test_note_on_basic)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = { 0x90, 60, 100 };	/* CH1, note 60, vel 100 */

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_channel, 0, "Expected CH1 (0x00)");
	zassert_equal(g_last_note, 60, "Expected note 60");
	zassert_equal(g_last_vel, 100, "Expected velocity 100");
}

/* NOTE ON with velocity 0 -> NOTE OFF */
ZTEST(midi1_parser, test_note_on_velocity_zero_as_off)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = { 0x90, 60, 0 };	/* CH1, note 60, vel 0 */

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_last_note, 60, "Expected note 60");
	zassert_equal(g_last_vel, 0, "Expected velocity 0");
}

/* Running status for NOTE ON */
ZTEST(midi1_parser, test_running_status_note_on)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = {
		0x94, 60, 100,	/* status + 2 data */
		62, 110,	/* running status NOTE ON */
		64, 120		/* running status NOTE ON */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_channel, 4, "Expected CH5 (0x04)");
	zassert_equal(g_last_note, 64, "Expected last note 64");
	zassert_equal(g_last_vel, 120, "Expected last velocity 120");
}

/* Control Change with running status */
ZTEST(midi1_parser, test_control_change_running_status)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = {
		0xB0, 1, 64,	/* CC1 = 64 */
		1, 127		/* running status CC1 = 127 */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_last_ctrl, 1, "Expected controller 1");
	zassert_equal(g_last_val, 127, "Expected value 127");
}

/* Realtime interleaving should not break running status */
ZTEST(midi1_parser, test_realtime_interleave)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = {
		0x90, 60, 100,	/* NOTE ON */
		0xF8,		/* TIMING CLOCK (realtime) */
		62, 110		/* running status NOTE ON */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_last_rt, 0xF8, "Expected realtime 0xF8");
	zassert_equal(g_last_note, 62, "Expected note 62");
	zassert_equal(g_last_vel, 110, "Expected velocity 110");
}

/* Pitch wheel test */
ZTEST(midi1_parser, test_pitchwheel)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = {
		0xE0, 0x00, 0x40	/* center position: LSB=0, MSB=64 */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_pw_lsb, 0x00, "Expected LSB 0x00");
	zassert_equal(g_pw_msb, 0x40, "Expected MSB 0x40");
}

/* Data byte with no running status should be ignored */
ZTEST(midi1_parser, test_data_without_running_status_ignored)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	uint8_t seq[] = { 0x40, 0x41, 0x42 };	/* all data bytes, no status */

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_last_note, 0, "Expected no note callback");
	zassert_equal(g_last_vel, 0, "Expected no velocity callback");
}

ZTEST(midi1_parser, test_running_status_modwheel_sweep)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	/* First message sets running status: CC1 = 0 */
	uint8_t first[] = { 0xB0, 1, 0 };	/* CC1, value 0 */

	feed_and_parse(&inst, first, sizeof(first));

	/* Verify first callback */
	zassert_equal(g_last_ctrl, 1, "Expected controller 1");
	zassert_equal(g_last_val, 0, "Expected value 0");

	/* Now sweep CC1 from 1 → 127 using running status */
	for (uint8_t v = 1; v <= 127; v++) {
		uint8_t data[] = { 1, v };	/* controller=1, value=v */
		feed_and_parse(&inst, data, sizeof(data));

		/* Verify callback for each step */
		zassert_equal(g_last_ctrl, 1, "Expected controller 1");
		zassert_equal(g_last_val, v, "Expected value %d", v);
	}
}

ZTEST(midi1_parser, test_brutal_interleave_realtime_and_sysex)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	/* We will track the last CC and last note */
	g_last_ctrl = 0;
	g_last_val = 0;
	g_last_note = 0;
	g_last_vel = 0;
	g_last_rt = 0;

	/* Brutal mixed stream:
	 *
	 * 1. CC1 = 0 (sets running status)
	 * 2. CC1 = 1 (running status)
	 * 3. Realtime F8
	 * 4. CC1 = 2 (running status)
	 * 5. SysEx start F0
	 * 6. SysEx data bytes
	 * 7. Realtime inside SysEx (legal)
	 * 8. More SysEx data
	 * 9. SysEx end F7
	 * 10. CC1 = 3 (running status should resume)
	 * 11. NOTE ON burst to verify parser recovery
	 */

	uint8_t seq[] = {
		/* CC1 = 0 */
		0xB0, 1, 0,

		/* CC1 = 1 (running status) */
		1, 1,

		/* Realtime interleave */
		0xF8,

		/* CC1 = 2 (running status) */
		1, 2,

		/* SysEx start */
		0xF0,
		0x7D, 0x10, 0x20,	/* manufacturer + payload */

		/* Realtime inside SysEx */
		0xFA,

		/* More SysEx data */
		0x33, 0x44,

		/* SysEx end */
		0xF7,

		/* Running status does NOT resume — must send new status byte */
		0xB0, 1, 3,

		/* NOTE ON burst to test recovery */
		0x90, 60, 100,
		62, 110,	/* running status NOTE ON */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	/* Validate last CC */
	zassert_equal(g_last_ctrl, 1, "Expected controller 1");
	zassert_equal(g_last_val, 3, "Expected CC value 3 after SysEx");

	/* Validate realtime inside SysEx */
	zassert_equal(g_last_rt, 0xFA, "Expected realtime FA inside SysEx");

	/* Validate NOTE ON recovery */
	zassert_equal(g_last_note, 62, "Expected last note 62");
	zassert_equal(g_last_vel, 110, "Expected last velocity 110");
}

ZTEST(midi1_parser, test_sysex_missing_f7)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);

	g_last_note = 0;
	g_last_vel = 0;

	uint8_t seq[] = {
	0xF0, 0x7D, 0x55, 0x66,   /* SysEx start + data */
	0x90, 60, 100,            /* NOTE ON should implicitly terminate SysEx */
	62, 110,                  /* running status NOTE ON */
	};

	feed_and_parse(&inst, seq, sizeof(seq));

	zassert_equal(g_last_note, 62, "Expected last note 62");
	zassert_equal(g_last_vel, 110, "Expected last velocity 110");
}

ZTEST(midi1_parser, test_sysex_0_to_127)
{
	struct midi1_serial_inst inst;
	midi_test_inst_init(&inst);
	
	/* Build SysEx message: F0, 0..127, F7 */
	uint8_t seq[130];
	int idx = 0;
	
	seq[idx++] = 0xF0;   /* SysEx start */
	seq[idx++] = 0x7D;   /* non commercial use vendor id */
	
	for (int i = 0; i < 128; i++) {
		seq[idx++] = i;
	}
	
	seq[idx++] = 0xF7;   /* SysEx end */
	
	/* Feed into parser */
	feed_and_parse(&inst, seq, idx);
	
	/* Validate length it's 129 because of the vendor id ! */
	zassert_equal(g_sysex_len, 129,
		      "Expected 129 SysEx data bytes, got %d", g_sysex_len);
	
	/* Validate content */
	zassert_equal(g_sysex_buf[0], 0x7D, "Expected vendor id 0x7D");
	for (int i = 1; i < 128; i++) {
		/*
		 * So it's i minus one because of the offset for the
		 * vendor id in the buffer
		 */
		zassert_equal(g_sysex_buf[i], i - 1,
			      "SysEx byte mismatch at index %d", i);
	}
}


/* Test suite entry point */
ZTEST_SUITE(midi1_parser, NULL, NULL, NULL, NULL, NULL);
