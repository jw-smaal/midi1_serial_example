#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include "midi1.h"
#include "note.h"
#include "midi1_serial.h"

const struct device *midi = DEVICE_DT_GET(DT_NODELABEL(midi0));

/* Captured callback values */
static uint8_t cb_chan;
static uint8_t cb_note;
static uint8_t cb_vel;
static uint8_t cb_ctl;
static uint8_t cb_val;
static uint8_t cb_rt;
static uint8_t cb_prog;
static uint8_t cb_press;
static uint8_t cb_poly_note;
static uint8_t cb_poly_press;

static uint8_t sysex_buf[128];
static int sysex_len;

/* ---- Callback implementations ---- */

static void cb_note_on(uint8_t ch, uint8_t n, uint8_t v)
{
	cb_chan = ch;
	cb_note = n;
	cb_vel = v;
}

static void cb_note_off(uint8_t ch, uint8_t n, uint8_t v)
{
	cb_chan = ch;
	cb_note = n;
	cb_vel = v;
}

static void cb_control_change(uint8_t ch, uint8_t ctl, uint8_t val)
{
	cb_chan = ch;
	cb_ctl = ctl;
	cb_val = val;
}

static void cb_pitchwheel(uint8_t ch, uint8_t lsb, uint8_t msb)
{
	cb_chan = ch;
	cb_val = lsb;
	cb_vel = msb;
}

static void cb_program_change(uint8_t ch, uint8_t num)
{
	cb_chan = ch;
	cb_prog = num;
}

static void cb_channel_aftertouch(uint8_t ch, uint8_t pressure)
{
	cb_chan = ch;
	cb_press = pressure;
}

static void cb_poly_aftertouch(uint8_t ch, uint8_t note, uint8_t pressure)
{
	cb_chan = ch;
	cb_poly_note = note;
	cb_poly_press = pressure;
}

static void cb_realtime(uint8_t msg)
{
	cb_rt = msg;
}

static void cb_sysex_start(void)
{
	sysex_len = 0;
}

static void cb_sysex_data(uint8_t d)
{
	sysex_buf[sysex_len++] = d;
}

static void cb_sysex_stop(void)
{
}

/* ---- Helper: push one byte into msgq ---- */

static void push_byte(uint8_t b)
{
	struct midi1_serial_data *data = midi->data;
	k_msgq_put(&data->msgq, &b, K_NO_WAIT);
	midi1_serial_receiveparser(midi);
}

/* ---- TESTS ---- */

ZTEST(midi1_serial, test_note_on)
{
	push_byte(0x90);        /* status: note on ch0 */
	push_byte(60);          /* note */
	push_byte(100);         /* velocity */

	zassert_equal(cb_chan, 0, "wrong channel");
	zassert_equal(cb_note, 60, "wrong note");
	zassert_equal(cb_vel, 100, "wrong velocity");
}

ZTEST(midi1_serial, test_note_off_zero_velocity)
{
	push_byte(0x90);
	push_byte(60);
	push_byte(0);           /* velocity zero → note_off */

	zassert_equal(cb_note, 60, "wrong note");
	zassert_equal(cb_vel, 0, "wrong velocity");
}

ZTEST(midi1_serial, test_control_change)
{
	push_byte(0xB0);
	push_byte(7);           /* controller */
	push_byte(99);          /* value */

	zassert_equal(cb_ctl, 7, "wrong controller");
	zassert_equal(cb_val, 99, "wrong value");
}

ZTEST(midi1_serial, test_pitchwheel)
{
	push_byte(0xE0);
	push_byte(0x34);        /* LSB */
	push_byte(0x12);        /* MSB */

	zassert_equal(cb_val, 0x34, "wrong LSB");
	zassert_equal(cb_vel, 0x12, "wrong MSB");
}

ZTEST(midi1_serial, test_program_change)
{
	push_byte(0xC0);
	push_byte(42);

	zassert_equal(cb_prog, 42, "wrong program");
}

ZTEST(midi1_serial, test_poly_aftertouch)
{
	push_byte(0xA0);
	push_byte(60);
	push_byte(55);

	zassert_equal(cb_poly_note, 60, "wrong note");
	zassert_equal(cb_poly_press, 55, "wrong pressure");
}

ZTEST(midi1_serial, test_realtime)
{
	push_byte(0xF8);        /* timing clock */

	zassert_equal(cb_rt, 0xF8, "wrong realtime msg");
}

ZTEST(midi1_serial, test_sysex)
{
	push_byte(0xF0);        /* start */
	push_byte(0x01);
	push_byte(0x02);
	push_byte(0x03);
	push_byte(0xF7);        /* end */

	zassert_equal(sysex_len, 3, "wrong sysex length");
	zassert_equal(sysex_buf[0], 0x01, "wrong sysex byte 0");
	zassert_equal(sysex_buf[1], 0x02, "wrong sysex byte 1");
	zassert_equal(sysex_buf[2], 0x03, "wrong sysex byte 2");
}

ZTEST_SUITE(midi1_serial, NULL, NULL, NULL, NULL, NULL);

void test_main(void)
{
	zassert_true(device_is_ready(midi), "device not ready");

	struct midi1_serial_callbacks cb = {
		.note_on = cb_note_on,
		.note_off = cb_note_off,
		.control_change = cb_control_change,
		.pitchwheel = cb_pitchwheel,
		.program_change = cb_program_change,
		.channel_aftertouch = cb_channel_aftertouch,
		.poly_aftertouch = cb_poly_aftertouch,
		.realtime = cb_realtime,
		.sysex_start = cb_sysex_start,
		.sysex_data = cb_sysex_data,
		.sysex_stop = cb_sysex_stop,
	};

	zassert_ok(midi1_serial_register_callbacks(midi, &cb));
}
