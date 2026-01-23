/**
 * @file midi1_serial.c
 * @brief MIDI USART implementation on Zephyr RTOS  
 *
 * @note 
 * MIDI USART implementation that also implements
 * MIDI "running status" on transmit (and receive).
 *
 * Not many MIDI MCU example implementations (that i have seen) do this.
 * It's a very useful method to limit messages on
 * serial UART MIDI channels that are only 31250 Baud. It reduces
 * playing latency. It's essential when working with real gear.
 *
 * The implementation intends to be as complete and close to the MIDI1.0
 * specifications as possible.
 *
 *  The MIDI USART implementation is for the Zephyr RTOS
 *  and uses the ring buffer and UART driver.
 *
 *  TODO:  - Error handling for the parser right now it's ignored.
 *  TODO:  - e.g. we could add DBG logging for this.
 *
 * Created in 2014 ported to Zephyr RTOS in 2024. 
 * @author Jan-Willem Smaal <usenet@gispen.org> 
 * updated 20241224
 * updated 20260103
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <errno.h>

#include "midi1.h"
#include "midi1_serial.h"

/*
 * Empty NO OP (noop) callbacks assigned if the caller leaves the callbacks
 * empty.  Gives the caller flexibillity in what to respond to.
 */
static inline void midi1_noop_note_on(uint8_t channel, uint8_t note,
                                      uint8_t velocity)
{
}

static inline void midi1_noop_note_off(uint8_t channel, uint8_t note,
                                       uint8_t velocity)
{
}

static inline void midi1_noop_control_change(uint8_t channel,
                                             uint8_t controller, uint8_t value)
{
}

static inline void midi1_noop_pitchwheel(uint8_t channel, uint8_t lsb,
                                         uint8_t msb)
{
}

static inline void midi1_noop_program_change(uint8_t channel, uint8_t number)
{
}

static inline void midi1_noop_channel_aftertouch(uint8_t channel,
                                                 uint8_t pressure)
{
}

static inline void midi1_noop_poly_aftertouch(uint8_t channel, uint8_t note,
                                              uint8_t pressure)
{
}

/* System common */
static inline void midi1_noop_realtime(uint8_t msg)
{
}

/* SysEx */
static inline void midi1_noop_sysex_start(void)
{
}

static inline void midi1_noop_sysex_data(uint8_t data)
{
}

static inline void midi1_noop_sysex_stop(void)
{
}

/* The ISR callback asssigned during init */
static void midi_serial_isr_callback(const struct device *dev,
                                      void *user_data);

#if 0
/**
 * Inits the serial USART with MIDI clock speed and
 */
int midi1_serial_init(struct midi1_serial_inst *inst)
{
	inst->running_status_rx = 0;
	inst->third_byte_flag = 0;
	inst->midi_c2 = 0;
	inst->midi_c3 = 0;

	inst->running_status_tx = 0;
	inst->running_status_tx_count = 0;
	inst->in_sysex = false;

	/* If a null pointer is given reassign to the NO OP function */
	if (!inst->note_on) {
		inst->note_on = midi1_noop_note_on;
	}
	if (!inst->note_off) {
		inst->note_off = midi1_noop_note_off;
	}
	if (!inst->control_change) {
		inst->control_change = midi1_noop_control_change;
	}
	if (!inst->realtime) {
		inst->realtime = midi1_noop_realtime;
	}
	if (!inst->pitchwheel) {
		inst->pitchwheel = midi1_noop_pitchwheel;
	}
	if (!inst->program_change) {
		inst->program_change = midi1_noop_program_change;
	}
	if (!inst->channel_aftertouch) {
		inst->channel_aftertouch = midi1_noop_channel_aftertouch;
	}
	if (!inst->poly_aftertouch) {
		inst->poly_aftertouch = midi1_noop_poly_aftertouch;
	}
	if (!inst->sysex_start) {
		inst->sysex_start = midi1_noop_sysex_start;
	}
	if (!inst->sysex_data) {
		inst->sysex_data = midi1_noop_sysex_data;
	}
	if (!inst->sysex_stop) {
		inst->sysex_stop = midi1_noop_sysex_stop;
	}

	/* Assign a MSQ to this instance */
	k_msgq_init(&inst->msgq, inst->msgq_buffer, MSG_SIZE, MSGQ_SIZE);

	if (!device_is_ready(inst->uart)) {
		printk("UART device not found!");
		return -1;
	}
	int ret = uart_irq_callback_user_data_set(inst->uart,
	                                          midi1_serial_isr_callback,
	                                          inst);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk
			    ("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART  does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return ret;
	}

	uart_irq_rx_enable(inst->uart);
	printk("midi1_serial_init() done");
	return 0;
}
#endif 

/**
 * Inits the serial USART ISR routine so we start processing incoming MIDI.
 */
int midi_serial_init(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;

	data->running_status_rx = 0;
	data->third_byte_flag = 0;
	data->midi_c2 = 0;
	data->midi_c3 = 0;

	data->running_status_tx = 0;
	data->running_status_tx_count = 0;
	data->in_sysex = false;
	/* 
	 * Set this to 500 ms in the past so the first time we start
	 * we assume status byte needs to be sent 
	 */
	data->last_status_tx_time = k_uptime_get_32() - 500U;

	/* If a null pointer is given reassign to the NO OP function */
	if (!data->note_on) {
		data->note_on = midi1_noop_note_on;
	}
	if (!data->note_off) {
		data->note_off = midi1_noop_note_off;
	}
	if (!data->control_change) {
		data->control_change = midi1_noop_control_change;
	}
	if (!data->realtime) {
		data->realtime = midi1_noop_realtime;
	}
	if (!data->pitchwheel) {
		data->pitchwheel = midi1_noop_pitchwheel;
	}
	if (!data->program_change) {
		data->program_change = midi1_noop_program_change;
	}
	if (!data->channel_aftertouch) {
		data->channel_aftertouch = midi1_noop_channel_aftertouch;
	}
	if (!data->poly_aftertouch) {
		data->poly_aftertouch = midi1_noop_poly_aftertouch;
	}
	if (!data->sysex_start) {
		data->sysex_start = midi1_noop_sysex_start;
	}
	if (!data->sysex_data) {
		data->sysex_data = midi1_noop_sysex_data;
	}
	if (!data->sysex_stop) {
		data->sysex_stop = midi1_noop_sysex_stop;
	}

	/* Assign a MSQ to this instance */
	k_msgq_init(&data->msgq, data->msgq_buffer, MSG_SIZE, MSGQ_SIZE);


	if (!device_is_ready(cfg->uart)) {
		printk("UART device not found!");
		return -1;
	}
	int ret = uart_irq_callback_user_data_set(cfg->uart,
	                                          midi_serial_isr_callback,
	                                          data);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk
			    ("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART  does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return ret;
	}

	uart_irq_rx_enable(cfg->uart);
	return 0;
}

/* TODO: add to driver public API */
int midi1_serial_register_callbacks_old(const struct device *dev,
                                         struct midi1_serial_callbacks *cb)
{
	struct midi1_serial_data *data = dev->data;
	/* TODO: check all callbacks and assign a NOOP function if the user left it empty */ 
	if (cb) {	
		data->cb = cb;
		return 0; 
	}else {
		return -EINVAL;
	}
}

/*
 * So the assumption is that the 'init' has assigned NOOP functions.
 * so we won't repeat that here.
 */
int midi1_serial_register_callbacks(const struct device *dev,
                                    struct midi1_serial_callbacks *cb)
{
	if (!cb) {
		return -EINVAL;
	}

	struct midi1_serial_data *data = dev->data;

	/* We want to do a deep check on every callback */
	if (cb->note_on){
		(data->cb)->note_on = cb->note_on;
	}
	if (cb->note_off) {
		(data->cb)->note_off = cb->note_off;
	}
	if (cb->control_change){
		(data->cb)->control_change = cb->control_change;
	}
	if (cb->pitchwheel){
		(data->cb)->pitchwheel = cb->pitchwheel;
	}
	if (cb->program_change){
		(data->cb)->program_change = cb->program_change;
	}
	if (cb->channel_aftertouch){
		(data->cb)->channel_aftertouch = cb->channel_aftertouch;
	}
	if (cb->poly_aftertouch){
		(data->cb)->poly_aftertouch = cb->poly_aftertouch;
	}
	if (cb->realtime){
		(data->cb)->realtime = cb->realtime;
	}
	if (cb->sysex_start){
		(data->cb)->sysex_start = cb->sysex_start;
	}
	if (cb->sysex_data){
		(data->cb)->sysex_data = cb->sysex_data;
	}
	if (cb->sysex_stop){
		(data->cb)->sysex_stop = cb->sysex_stop;
	}

	return 0;
}


/*
 * All functions related to sending MIDI messages to the serial USART
 */

/* Channel mode messages */
/* ___ _                       _   __  __         _
  / __| |_  __ _ _ _  _ _  ___| | |  \/  |___  __| |___
 | (__| ' \/ _` | ' \| ' \/ -_) | | |\/| / _ \/ _` / -_)
  \___|_||_\__,_|_||_|_||_\___|_| |_|  |_\___/\__,_\___|
 */
void midi1_serial_note_on(struct midi1_serial_inst *inst, uint8_t channel,
                          uint8_t key, uint8_t velocity)
{
	/* If we have sent 16 times without a status byte; send a status byte */
	if (inst->running_status_tx_count >= 16) {
		inst->running_status_tx_count = 0;
		uart_poll_out(inst->uart, C_NOTE_ON | channel);
		inst->running_status_tx = C_NOTE_ON | channel;
	}
	/* If we don't have running status send out the status byte. */
	else if ((C_NOTE_ON | channel) != inst->running_status_tx) {
		uart_poll_out(inst->uart, C_NOTE_ON | channel);
		inst->running_status_tx = C_NOTE_ON | channel;
		inst->running_status_tx_count = 0;
	}
	uart_poll_out(inst->uart, key);
	uart_poll_out(inst->uart, velocity);
	inst->running_status_tx_count++;
}

/*
 * @brief helper to decide if we need to transmit the status byte 
 *
 * Even though we keep running status on TX we retransmit every 16'th
 * time to make sure the receiver is in sync even when some messages
 * are lost. MIDI recommendations are also if no message was sent in the last
 * 300ms to also include the status byte.
 * Running status is most important for smooth control changes.  
 *
 * @note CONFIG_MIDI1_SERIAL_RUNNING_STATUS_TIMEOUT_MS  is specified in 
 * the Kconfig. 
 */
#define RUNSTAT_TMOUT CONFIG_MIDI1_SERIAL_RUNNING_STATUS_TIMEOUT_MS
static bool midi1_need_status(struct midi1_serial_data *data, uint8_t status)
{
	uint32_t now = k_uptime_get_32();

	bool need_status =
	    /* 1. Timeout expired */
	    (now - data->last_status_tx_time > RUNSTAT_TMOUT) ||
	    /* 2. Status changed */
	    (status != data->running_status_tx) ||
	    /* 3. Too many data bytes without status */
	    (data->running_status_tx_count >= 16);

	return need_status;
}

void midi_serial_note_on(const struct device *dev,
                         uint8_t channel, uint8_t key, uint8_t velocity)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint32_t now = k_uptime_get_32();

	if (midi1_need_status(data, C_NOTE_ON | channel)) {
		uart_poll_out(cfg->uart, C_NOTE_ON | channel);
		data->running_status_tx = C_NOTE_ON | channel;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = now;
	}
	uart_poll_out(cfg->uart, key);
	uart_poll_out(cfg->uart, velocity);
	data->running_status_tx_count++;
}

void midi1_serial_note_off(struct midi1_serial_inst *inst, uint8_t channel,
                           uint8_t key, uint8_t velocity)
{
	/* If we have sent 16 times without a status byte; send a status byte */
	if (inst->running_status_tx_count >= 16) {
		inst->running_status_tx_count = 0;
		uart_poll_out(inst->uart, C_NOTE_OFF | channel);
		inst->running_status_tx = C_NOTE_OFF | channel;
	}
	/* If we don't have running status send out the status byte. */
	else if ((C_NOTE_OFF | channel) != inst->running_status_tx) {
		uart_poll_out(inst->uart, C_NOTE_OFF | channel);
		inst->running_status_tx = C_NOTE_OFF | channel;
		inst->running_status_tx_count = 0;
	}
	uart_poll_out(inst->uart, key);
	uart_poll_out(inst->uart, velocity);
	inst->running_status_tx_count++;
}

void midi_serial_note_off(const struct device *dev,
                          uint8_t channel, uint8_t key, uint8_t velocity)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint32_t now = k_uptime_get_32();

	if (midi1_need_status(data, C_NOTE_OFF | channel)) {
		uart_poll_out(cfg->uart, C_NOTE_OFF | channel);
		data->running_status_tx = C_NOTE_OFF | channel;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = now;
	}
	uart_poll_out(cfg->uart, key);
	uart_poll_out(cfg->uart, velocity);
	data->running_status_tx_count++;
}

void midi1_serial_control_change(struct midi1_serial_inst *inst,
                                 uint8_t channel,
                                 uint8_t controller, uint8_t val)
{
	/* If we have sent 16 times without a status byte; send a status byte */
	if (inst->running_status_tx_count >= 16) {
		inst->running_status_tx_count = 0;
		uart_poll_out(inst->uart, C_CONTROL_CHANGE | channel);
		inst->running_status_tx = C_CONTROL_CHANGE | channel;
	}
	/* If we don't have running status send out the status byte. */
	else if ((C_CONTROL_CHANGE | channel) != inst->running_status_tx) {
		uart_poll_out(inst->uart, C_CONTROL_CHANGE | channel);
		inst->running_status_tx = C_CONTROL_CHANGE | channel;
		inst->running_status_tx_count = 0;
	}
	/* We always send out controller and value */
	uart_poll_out(inst->uart, controller);
	uart_poll_out(inst->uart, val);
	inst->running_status_tx_count++;
}

void midi_serial_control_change(const struct device *dev,
                                uint8_t channel,
                                uint8_t controller, uint8_t val)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint32_t now = k_uptime_get_32();

	if (midi1_need_status(data, C_CONTROL_CHANGE | channel)) {
		uart_poll_out(cfg->uart, C_CONTROL_CHANGE | channel);
		data->running_status_tx = C_CONTROL_CHANGE | channel;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = now;
	}
	/* We always send out controller and value */
	uart_poll_out(cfg->uart, controller);
	uart_poll_out(cfg->uart, val);
	data->running_status_tx_count++;

}

void midi1_serial_channelaftertouch(struct midi1_serial_inst *inst,
                                    uint8_t channel, uint8_t val)
{
	/* If we have sent 16 times without a status byte; send a status byte */
	if (inst->running_status_tx_count >= 16) {
		inst->running_status_tx_count = 0;
		uart_poll_out(inst->uart, C_CHANNEL_AFTERTOUCH | channel);
		inst->running_status_tx = C_CHANNEL_AFTERTOUCH | channel;
	}
	/* If we don't have running status send out the status byte. */
	else if ((C_CHANNEL_AFTERTOUCH | channel) != inst->running_status_tx) {
		uart_poll_out(inst->uart, C_CHANNEL_AFTERTOUCH | channel);
		inst->running_status_tx = C_CHANNEL_AFTERTOUCH | channel;
		inst->running_status_tx_count = 0;
	}
	uart_poll_out(inst->uart, val);
}

void midi_serial_channelaftertouch(const struct device *dev,
                                   uint8_t channel, uint8_t val)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint32_t now = k_uptime_get_32();

	if (midi1_need_status(data, C_CHANNEL_AFTERTOUCH | channel)) {
		uart_poll_out(cfg->uart, C_CHANNEL_AFTERTOUCH | channel);
		data->running_status_tx = C_CHANNEL_AFTERTOUCH | channel;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = now;
	}
	uart_poll_out(cfg->uart, val);
	data->running_status_tx_count++;
}

void midi1_serial_modwheel(struct midi1_serial_inst *inst,
                           uint8_t channel, uint16_t val)
{
	midi1_serial_control_change(inst,
	                            channel,
	                            CTL_MSB_MODWHEEL,
	                            ~(CHANNEL_VOICE_MASK) & (val >> 7));
	midi1_serial_control_change(inst,
	                            channel,
	                            CTL_LSB_MODWHEEL,
	                            ~(CHANNEL_VOICE_MASK) & val);
}

void midi_serial_modwheel(const struct device *dev,
                          uint8_t channel, uint16_t val)
{
	midi_serial_control_change(dev,
	                           channel,
	                           CTL_MSB_MODWHEEL,
	                           ~(CHANNEL_VOICE_MASK) & (val >> 7));
	midi_serial_control_change(dev,
	                           channel,
	                           CTL_LSB_MODWHEEL,
	                           ~(CHANNEL_VOICE_MASK) & val);
}

void midi1_serial_pitchwheel(struct midi1_serial_inst *inst,
                             uint8_t channel, uint16_t val)
{
	/* If we have sent 16 times without a status byte; send a status byte */
	if (inst->running_status_tx_count >= 16) {
		inst->running_status_tx_count = 0;
		uart_poll_out(inst->uart, C_PITCH_WHEEL | channel);
		inst->running_status_tx = C_PITCH_WHEEL | channel;
	}
	/* If we don't have running status send out the status byte. */
	else if ((C_PITCH_WHEEL | channel) != inst->running_status_tx) {
		uart_poll_out(inst->uart, C_PITCH_WHEEL | channel);
		inst->running_status_tx = C_PITCH_WHEEL | channel;
		inst->running_status_tx_count = 0;
	}
	/* Value is 14 bits so need to shift 7 */
	uart_poll_out(inst->uart, val & ~(CHANNEL_VOICE_MASK)); /* LSB */
	uart_poll_out(inst->uart, (val >> 7) & ~(CHANNEL_VOICE_MASK));  /* MSB */

}

void midi_serial_pitchwheel(const struct device *dev,
                            uint8_t channel, uint16_t val)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint32_t now = k_uptime_get_32();

	if (midi1_need_status(data, C_PITCH_WHEEL | channel)) {
		uart_poll_out(cfg->uart, C_PITCH_WHEEL | channel);
		data->running_status_tx = C_PITCH_WHEEL | channel;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = now;
	}

	/* Value is 14 bits so need to shift 7 */
	uart_poll_out(cfg->uart, val & ~(CHANNEL_VOICE_MASK));  /* LSB */
	uart_poll_out(cfg->uart, (val >> 7) & ~(CHANNEL_VOICE_MASK));   /* MSB */
	data->running_status_tx_count++;
}

/* System Common messages */
/*___         _                ___
 / __|_  _ __| |_ ___ _ __    / __|___ _ __  _ __  ___ _ _
 \__ \ || (_-<  _/ -_) '  \  | (__/ _ \ '  \| '  \/ _ \ ' \
 |___/\_, /__/\__\___|_|_|_|  \___\___/_|_|_|_|_|_\___/_||_|
 */
void midi1_serial_timingclock(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_TIMING_CLOCK);
}

void midi_serial_timingclock(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_TIMING_CLOCK);
}

void midi1_serial_start(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_START);
}

void midi_serial_start(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_START);
}

void midi1_serial_continue(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_CONTINUE);
}

void midi_serial_continue(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_CONTINUE);
}

void midi1_serial_stop(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_STOP);
}

void midi_serial_stop(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_STOP);
}

void midi1_serial_active_sensing(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_ACTIVE_SENSING);
}

void midi_serial_active_sensing(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_ACTIVE_SENSING);
}

void midi1_serial_reset(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, RT_RESET);
}

void midi_serial_reset(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, RT_RESET);
}

/* System exclusive messages */
/*___         _               ___        _         _
 / __|_  _ __| |_ ___ _ __   | __|_ ____| |_  _ __(_)_ _____
 \__ \ || (_-<  _/ -_) '  \  | _|\ \ / _| | || (_-< \ V / -_)
 |___/\_, /__/\__\___|_|_|_| |___/_\_\__|_|\_,_/__/_|\_/\___|
 |__/
 */
void midi1_sysex_start(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, SYSTEM_EXCLUSIVE_START);
}

void midi_sysex_start(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, SYSTEM_EXCLUSIVE_START);
}

void midi1_sysex_char(struct midi1_serial_inst *inst, uint8_t c)
{
	/* Sysex data should be 0 -> 127U so ignore everyhing else */
	if (!(c & CHANNEL_VOICE_MASK)) {
		uart_poll_out(inst->uart, c);
	}
}

void midi_sysex_char(const struct device *dev, uint8_t c)
{
	const struct midi1_serial_config *cfg = dev->config;
	/* Sysex data should be 0 -> 127U so ignore everyhing else */
	if (!(c & CHANNEL_VOICE_MASK)) {
		uart_poll_out(cfg->uart, c);
	}
}

void midi1_sysex_data_bulk(struct midi1_serial_inst *inst,
                           const uint8_t *data, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++) {
		midi1_sysex_char(inst, data[i]);
	}
}

void midi_sysex_data_bulk(const struct device *dev,
                          const uint8_t *data, uint32_t len)
{
	const struct midi1_serial_config *cfg = dev->config;
	for (uint32_t i = 0; i < len; i++) {
		midi_sysex_char(dev, data[i]);
	}
}

void midi1_sysex_stop(struct midi1_serial_inst *inst)
{
	uart_poll_out(inst->uart, SYSTEM_EXCLUSIVE_END);
}

void midi_sysex_stop(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	uart_poll_out(cfg->uart, SYSTEM_EXCLUSIVE_END);
}

/*__  __ ___ ___ ___   ___      _         ___
 |  \/  |_ _|   \_ _| | _ )_  _| |_ ___  | _ \__ _ _ _ ___ ___ _ _
 | |\/| || || |) | |  | _ \ || |  _/ -_) |  _/ _` | '_(_-</ -_) '_|
 |_|  |_|___|___/___| |___/\_, |\__\___| |_| \__,_|_| /__/\___|_|
 |__/
 */

/**
 * @brief MIDI Byte Receive parser implementation - Interrupt Service Routine
 *
 * @param device device pointer
 * @param user_data user data (in our case the instance struct)
 * @note we use a Message Queue FIFO buffer to store the incoming MIDI messages
 */
#if  0
static void midi1_serial_isr_callback(const struct device *dev, void *user_data)
{
	uint8_t c;
	/* Need to cast the user data to our instance struct */
	struct midi1_serial_inst *inst = (struct midi1_serial_inst *)user_data;

	if (!uart_irq_update(inst->uart)) {
		return;
	}
	if (!uart_irq_rx_ready(inst->uart)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(inst->uart, &c, 1) == 1) {
		if (k_msgq_put(&inst->msgq, &c, K_NO_WAIT) != 0) {
			/*
			 * Message queue is full
			 * TODO: add ERROR logging
			 * right now we drop MIDI packets
			 * as the application is not processing
			 * the incoming packets.  We can't keep
			 * buffering forever so we drop messages.
			 */
		}
	}
}
#endif 

static void midi_serial_isr_callback(const struct device *dev, void *user_data)
{
	uint8_t c;

	/*
	 * We can take the pointer to the uart from both 'dev' and 'user_data'
	 * decided for readability to take it from the 'midi1_serial_config'
	 * struct as we use that throughout the driver.
	 */
	const struct device *midi1_serial = (const struct device *)user_data;
	const struct midi1_serial_config *cfg = midi1_serial->config;
	struct midi1_serial_data *data = midi1_serial->data;

	if (!uart_irq_update(cfg->uart)) {
		return;
	}
	if (!uart_irq_rx_ready(cfg->uart)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(cfg->uart, &c, 1) == 1) {
		if (k_msgq_put(&data->msgq, &c, K_NO_WAIT) != 0) {
			/*
			 * Message queue is full
			 * TODO: add ERROR logging
			 * right now we drop MIDI packets
			 * as the application is not processing
			 * the incoming packets.  We can't keep
			 * buffering forever so we drop messages.
			 */
		}
	}
}

void midi_serial_receiveparser(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;
	uint8_t c;

	/*
	 * Read only _one_ byte from the circular FIFO input buffer
	 * This buffer is filled by the ISR routine on receipt of
	 * data on the port.
	 */
	if (k_msgq_get(&data->msgq, &c, K_FOREVER) != 0) {
		return;
	} else {
		/* Valid message received ! */
#if MIDI1_SERIAL_DEBUG
		printk("%2X ", c);
#endif
	}

	/*
	 * Future implementation option
	 * To allow software MIDI THRU ("kind of" with some processing delay)
	 * simply write what is received to the output.
	 * uart_poll_out(midi, c);
	 */

	/* Check if bit7 = 1 */
	if (c & CHANNEL_VOICE_MASK) {
		/*
		 * is it a real-time message?  0xF8 up to 0xFF
		 * decided not to create seperate callbacks for
		 * realtime messages.  This callback should check
		 * the 'enum midi_real_time' values and then
		 * decide if e.g. a clock is received or a RT_STOP etc..
		 */
		if (c >= SYSTEM_REALTIME_MASK) {
			data->realtime(c);
			return;
		} else {
			/* When a new status byte arrives inside Sysex
			 * reset the flag. (i.e. sysex start received
			 * but no stop).
			 */
			data->in_sysex = false;
			data->running_status_rx = c;
			data->third_byte_flag = 0;
			/* Is this a tune request */
			if (c == SYSTEM_TUNE_REQUEST) {
				data->midi_c2 = c;
				/* TODO: Process something. */
				return;
			} else if (c == SYSTEM_EXCLUSIVE_START) {
				data->in_sysex = true;
				data->sysex_start();
				return;
			} else if (c == SYSTEM_EXCLUSIVE_END) {
				data->in_sysex = false;
				data->sysex_stop();
				return;
			}
			/*
			 * Do nothing for any other bytes above 0xF8
			 */
			return;
		}
	} else {                /* Bit 7 == 0   (data) */
		if (data->in_sysex) {
			data->sysex_data(c);
			/* Stop processing the rest when in SysEx */
			return;
		}
		if (data->third_byte_flag == 1) {
			data->third_byte_flag = 0;
			data->midi_c3 = c;
			uint8_t common =
			    data->running_status_rx & SYSTEM_COMMON_MASK;
			uint8_t chan =
			    data->running_status_rx & ~(SYSTEM_COMMON_MASK);

			if (common == C_NOTE_ON) {
				if (data->midi_c3 == 0) {
					/*
					 * Some MIDI implementations use
					 * velocity zero "note on"
					 * as a "note-off". To take advantage
					 * of 'running status'.  Other do use a
					 * note off and the note off velocity
					 * actually can be used to alter the
					 * timbre of the note off.
					 * MIDI1.0 spec page A2.
					 */
					data->note_off(chan,
					               data->midi_c2,
					               data->midi_c3);
					return;
				} else {
					data->note_on(chan,
					              data->midi_c2,
					              data->midi_c3);
					return;
				}
				return;
			} else if (common == C_NOTE_OFF) {
				data->note_off(chan,
				               data->midi_c2, data->midi_c3);
				return;
			} else if (common == C_PITCH_WHEEL) {
				data->pitchwheel(chan,
				                 data->midi_c2, data->midi_c3);
				return;
			} else if (common == C_PROGRAM_CHANGE) {
				data->program_change(chan, data->midi_c2);
				return;
			} else if (common == C_POLYPHONIC_AFTERTOUCH) {
				data->poly_aftertouch(chan,
				                      data->midi_c2,
				                      data->midi_c3);
				return;
			} else if (common == C_CHANNEL_AFTERTOUCH) {
				data->channel_aftertouch(chan, data->midi_c2);
				return;
			} else if (common == C_CONTROL_CHANGE) {
				data->control_change(chan,
				                     data->midi_c2,
				                     data->midi_c3);
				return;
			} else {
				/* Ignore unknown */
				return;
			}
		} else {
			if (data->running_status_rx == 0) {
				/* Ignore data Byte if running status is  0 */
				return;
			} else {
				/* All 2 byte commands */
				if (data->running_status_rx < 0xC0) {
					data->third_byte_flag = 1;
					data->midi_c2 = c;
					/* At this stage we have only 1 byte out of 2. */
					return;
				} else if (data->running_status_rx < 0xE0) {
					/* All 1 byte commands */
					data->midi_c2 = c;
					/*
					 * TODO: !! Process callback/delegate
					 * for two bytes command.
					 */
					return;
				} else if (data->running_status_rx < 0xF0) {
					data->third_byte_flag = 1;
					data->midi_c2 = c;
				} else if (data->running_status_rx >= 0xF0) {
					if (data->running_status_rx == 0xF2) {
						data->running_status_rx = 0;
						data->third_byte_flag = 1;
						data->midi_c2 = c;
						return;
					} else if (data->running_status_rx >=
					           0xF0) {
						if (data->running_status_rx ==
						    0xF3
						    || data->running_status_rx
						    == 0xF3) {
							data->running_status_rx
							    = 0;
							data->midi_c2 = c;
							/*  TODO: !! Process callback for two bytes command. */
							return;
						} else {
							/* Ignore status */
							data->running_status_rx
							    = 0;
							return;
						}
					}
				}       /* end of data->running_status_rx >= 0xF0 */
			}       /* end of all 2 byte commands */
		}               /* end of  global_3rd_byte_flag */
	}                       /* end of data bit 7 == 0 */
}                               /* end of midi1_serial_receiveparser */

/**
 * @brief Parse one byte at a time for the MIDI parsing.
 *
 * Callback functions are called for each complete MIDI message.
 *
 * This function is long as the MIDI1.0 spec suggest how to implement the
 * parsing is also quite lenghty and we want to stay as close to the spec
 * as possible.  MIDI1.0 specification has this statemachine listed on
 * page A-3.
 */
void midi1_serial_receiveparser(struct midi1_serial_inst *inst)
{
	uint8_t c;

	/*
	 * Read only _one_ byte from the circular FIFO input buffer
	 * This buffer is filled by the ISR routine on receipt of
	 * data on the port.
	 */
	if (k_msgq_get(&inst->msgq, &c, K_FOREVER) != 0) {
		return;
	} else {
		/* Valid message received ! */
#if MIDI1_SERIAL_DEBUG
		printk("%2X ", c);
#endif
	}

	/*
	 * Future implementation option
	 * To allow software MIDI THRU ("kind of" with some processing delay)
	 * simply write what is received to the output.
	 * uart_poll_out(midi, c);
	 */

	/* Check if bit7 = 1 */
	if (c & CHANNEL_VOICE_MASK) {
		/*
		 * is it a real-time message?  0xF8 up to 0xFF
		 * decided not to create seperate callbacks for
		 * realtime messages.  This callback should check
		 * the 'enum midi_real_time' values and then
		 * decide if e.g. a clock is received or a RT_STOP etc..
		 */
		if (c >= SYSTEM_REALTIME_MASK) {
			inst->realtime(c);
			return;
		} else {
			/* When a new status byte arrives inside Sysex
			 * reset the flag. (i.e. sysex start received
			 * but no stop).
			 */
			inst->in_sysex = false;
			inst->running_status_rx = c;
			inst->third_byte_flag = 0;
			/* Is this a tune request */
			if (c == SYSTEM_TUNE_REQUEST) {
				inst->midi_c2 = c;
				/* TODO: Process something. */
				return;
			} else if (c == SYSTEM_EXCLUSIVE_START) {
				inst->in_sysex = true;
				inst->sysex_start();
				return;
			} else if (c == SYSTEM_EXCLUSIVE_END) {
				inst->in_sysex = false;
				inst->sysex_stop();
				return;
			}
			/*
			 * Do nothing for any other bytes above 0xF8
			 */
			return;
		}
	} else {                /* Bit 7 == 0   (data) */
		if (inst->in_sysex) {
			inst->sysex_data(c);
			/* Stop processing the rest when in SysEx */
			return;
		}
		if (inst->third_byte_flag == 1) {
			inst->third_byte_flag = 0;
			inst->midi_c3 = c;
			uint8_t common =
			    inst->running_status_rx & SYSTEM_COMMON_MASK;
			uint8_t chan =
			    inst->running_status_rx & ~(SYSTEM_COMMON_MASK);

			if (common == C_NOTE_ON) {
				if (inst->midi_c3 == 0) {
					/*
					 * Some MIDI implementations use
					 * velocity zero "note on"
					 * as a "note-off". To take advantage
					 * of 'running status'.  Other do use a
					 * note off and the note off velocity
					 * actually can be used to alter the
					 * timbre of the note off.
					 * MIDI1.0 spec page A2.
					 */
					inst->note_off(chan,
					               inst->midi_c2,
					               inst->midi_c3);
					return;
				} else {
					inst->note_on(chan,
					              inst->midi_c2,
					              inst->midi_c3);
					return;
				}
				return;
			} else if (common == C_NOTE_OFF) {
				inst->note_off(chan,
				               inst->midi_c2, inst->midi_c3);
				return;
			} else if (common == C_PITCH_WHEEL) {
				inst->pitchwheel(chan,
				                 inst->midi_c2, inst->midi_c3);
				return;
			} else if (common == C_PROGRAM_CHANGE) {
				inst->program_change(chan, inst->midi_c2);
				return;
			} else if (common == C_POLYPHONIC_AFTERTOUCH) {
				inst->poly_aftertouch(chan,
				                      inst->midi_c2,
				                      inst->midi_c3);
				return;
			} else if (common == C_CHANNEL_AFTERTOUCH) {
				inst->channel_aftertouch(chan, inst->midi_c2);
				return;
			} else if (common == C_CONTROL_CHANGE) {
				inst->control_change(chan,
				                     inst->midi_c2,
				                     inst->midi_c3);
				return;
			} else {
				/* Ignore unknown */
				return;
			}
		} else {
			if (inst->running_status_rx == 0) {
				/* Ignore data Byte if running status is  0 */
				return;
			} else {
				/* All 2 byte commands */
				if (inst->running_status_rx < 0xC0) {
					inst->third_byte_flag = 1;
					inst->midi_c2 = c;
					/* At this stage we have only 1 byte out of 2. */
					return;
				} else if (inst->running_status_rx < 0xE0) {
					/* All 1 byte commands */
					inst->midi_c2 = c;
					/*
					 * TODO: !! Process callback/delegate
					 * for two bytes command.
					 */
					return;
				} else if (inst->running_status_rx < 0xF0) {
					inst->third_byte_flag = 1;
					inst->midi_c2 = c;
				} else if (inst->running_status_rx >= 0xF0) {
					if (inst->running_status_rx == 0xF2) {
						inst->running_status_rx = 0;
						inst->third_byte_flag = 1;
						inst->midi_c2 = c;
						return;
					} else if (inst->running_status_rx >=
					           0xF0) {
						if (inst->running_status_rx ==
						    0xF3
						    || inst->running_status_rx
						    == 0xF3) {
							inst->running_status_rx
							    = 0;
							inst->midi_c2 = c;
							/*  TODO: !! Process callback for two bytes command. */
							return;
						} else {
							/* Ignore status */
							inst->running_status_rx
							    = 0;
							return;
						}
					}
				}       /* end of inst->running_status_rx >= 0xF0 */
			}       /* end of all 2 byte commands */
		}               /* end of  global_3rd_byte_flag */
	}                       /* end of data bit 7 == 0 */
}                               /* end of midi1_serial_receiveparser */

/* Zephyr device driver API link to our actual implementation */
static const struct midi1_serial_api midi1_serial_driver_api = {
	.register_callbacks = midi1_serial_register_callbacks,
	.note_on = midi_serial_note_on,
	.note_off = midi_serial_note_off,
	.control_change = midi_serial_control_change,
	.channelaftertouch = midi_serial_channelaftertouch,
	.modwheel = midi_serial_modwheel,
	.pitchwheel = midi_serial_pitchwheel,
	.timingclock = midi_serial_timingclock,
	.start = midi_serial_start,
	.continu = midi_serial_continue,
	.stop = midi_serial_stop,
	.active_sensing = midi_serial_active_sensing,
	.reset = midi_serial_reset,
	.sysex_start = midi_sysex_start,
	.sysex_char = midi_sysex_char,
	.sysex_data_bulk = midi_sysex_data_bulk,
	.sysex_stop = midi_sysex_stop
};

#define DT_DRV_COMPAT midi1_serial

#define MIDI1_SERIAL_DEFINE(inst)                                          \
static struct midi1_serial_data midi1_serial_data_##inst;              	   \
static const struct midi1_serial_config midi1_serial_config_##inst = {     \
.uart = DEVICE_DT_GET(DT_INST_PROP(inst, uart)),                           \
};                                                                         \
DEVICE_DT_INST_DEFINE(inst,                                                \
midi_serial_init,                                                          \
NULL,                                                                      \
&midi1_serial_data_##inst,                                                 \
&midi1_serial_config_##inst,                                               \
POST_KERNEL,                                                               \
CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                        \
&midi1_serial_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MIDI1_SERIAL_DEFINE)
/* EOF */
