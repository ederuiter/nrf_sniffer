/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <nrf_802154_const.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <dk_buttons_and_leds.h>
#include "./sniffer.h"

#define HEX_STRING_LENGTH (2 * MAX_PACKET_SIZE + 1)

static const struct shell *uart_shell;
static char hex_string[HEX_STRING_LENGTH];
static bool heartbeat_led_state;
static bool packet_led_state;
static k_timeout_t heartbeat_interval;
static k_timeout_t sweep_interval;
static uint32_t sweep_channel_end;
static void heartbeat(struct k_work *work);
static void sweep(struct k_work *work);
static sniffer_t *sniffer;

static K_WORK_DELAYABLE_DEFINE(heartbeat_work, heartbeat);
static K_WORK_DELAYABLE_DEFINE(sweep_work, sweep);

void log_packet(
	uint8_t *psdu,
    size_t length,
	uint8_t lqi,
	int8_t rssi,
	uint64_t timestamp
) {
	packet_led_state = !packet_led_state;
	dk_set_led(DK_LED4, packet_led_state);
	bin2hex(psdu, length, hex_string, HEX_STRING_LENGTH);

	shell_print(uart_shell,
			"received: %s power: %d lqi: %u time: %llu",
			hex_string,
			rssi,
			lqi,
			timestamp);
}

static void heartbeat(struct k_work *work)
{
	ARG_UNUSED(work);

	heartbeat_led_state = !heartbeat_led_state;
	dk_set_led(DK_LED1, heartbeat_led_state);
	k_work_reschedule(&heartbeat_work, heartbeat_interval);
}

static void sweep(struct k_work *work)
{
	ARG_UNUSED(work);
	
	sniffer->stop();
	uint32_t channel = sniffer->get_channel();
	if (channel == sweep_channel_end) {
		shell_print(uart_shell, "done sweeping");
		
		heartbeat_interval = K_SECONDS(1);

		packet_led_state = false;
		dk_set_led(DK_LED4, packet_led_state);
		
		return;
	}
	shell_print(uart_shell, "now listening on channel: %d", channel+1);

	sniffer->set_channel(channel+1);
	sniffer->start();

	k_work_reschedule(&sweep_work, sweep_interval);
}

static int cmd_channel(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t channel;

	switch (argc) {
	case 1:
		shell_print(shell, "%d", sniffer->get_channel());
		break;
	case 2:
		channel = atoi(argv[1]);
		sniffer->set_channel(channel);
		break;
	default:
		shell_print(shell, "invalid number of parameters: %d", argc);
		break;
	}

	return 0;
}
SHELL_CMD_ARG_REGISTER(channel, NULL, "Set radio channel", cmd_channel, 1, 1);

static int cmd_toggle(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	packet_led_state = !packet_led_state;
	dk_set_led(DK_LED4, packet_led_state);

	return 0;
}
SHELL_CMD_ARG_REGISTER(toggle, NULL, "Toggle led", cmd_toggle, 1, 0);

static int cmd_sweep(const struct shell *shell, size_t argc, char **argv)
{
    uint32_t channel_min = sniffer->get_channel_min();
	uint32_t channel_max = sniffer->get_channel_max();
	uint32_t channel_start = channel_min;
	uint32_t channel_end = channel_max;
    uint32_t interval = 30;

	switch (argc) {
	case 4:
		channel_end = atoi(argv[3]);
	case 3:
		channel_start = atoi(argv[2]);
	case 2:
		interval = atoi(argv[1]);
		break;
	default:
		shell_print(shell, "invalid number of parameters: %d", argc);
		return 0;
	}

	if (channel_end < channel_start) {
		shell_print(shell, "end channel should be > start channnel");
		return 0;
	}

	if (channel_end > channel_max || channel_start < channel_min) {
		shell_print(shell, "channels should be between %d and %d", channel_min, channel_max);
		return 0;
	}

	shell_print(uart_shell, "starting sweep from channel %d-%d (%d second interval)", channel_start, channel_end, interval);
	shell_print(uart_shell, "now listening on channel: %d", channel_start);

    sweep_interval = K_SECONDS(interval);
	heartbeat_interval = K_MSEC(100);
	sweep_channel_end = channel_end;
	sniffer->set_channel(channel_start);
	sniffer->start();

	k_work_reschedule(&sweep_work, sweep_interval);

	return 0;
}
SHELL_CMD_ARG_REGISTER(sweep, NULL, "Sweep trough channels [secs] [start] [end]", cmd_sweep, 1, 3);

static int cmd_receive(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	heartbeat_interval = K_MSEC(250);
	sniffer->start();

	return 0;
}
SHELL_CMD_ARG_REGISTER(receive, NULL, "Put radio in receive state", cmd_receive, 1, 0);

static int cmd_sleep(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	heartbeat_interval = K_SECONDS(1);
	sniffer->stop();

	packet_led_state = false;
	dk_set_led(DK_LED4, packet_led_state);
	
	k_work_cancel_delayable(&sweep_work);

	return 0;
}
SHELL_CMD_ARG_REGISTER(sleep, NULL, "Disable the radio", cmd_sleep, 1, 0);

int main(void)
{
	(void) dk_leds_init();

	uart_shell = shell_backend_uart_get_ptr();
	heartbeat_interval = K_SECONDS(1);
	sweep_interval = K_SECONDS(30);
	k_work_reschedule(&heartbeat_work, heartbeat_interval);

    sniffer = &sniffer_zephyr;
    sniffer->init();

	return 0;
}
