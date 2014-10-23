/***************************************************************
 *
 * OpenBeacon.org - nRF51 2.4GHz Radio Routines
 *
 * Copyright 2013 Milosch Meriac <meriac@openbeacon.de>
 *
 ***************************************************************

 This file is part of the OpenBeacon.org active RFID firmware

 OpenBeacon is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenBeacon is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

*/
#include <openbeacon.h>
#include <main.h>
#include <radio.h>
#include <timer.h>

#define BLE_ADDRESS 0x8E89BED6UL
#define BLE_PREFIX_SIZE 9
#define BLE_POSTFIX (BLE_PREFIX_SIZE+2)

typedef struct {
	uint8_t channel;
	uint8_t frequency;
} TMapping;

static int g_pkt_count = 0;
static uint8_t g_advertisment_index;
static uint8_t g_pkt_buffer[64];

static const TMapping g_advertisment[] = {
	{37,  2},
	{38, 26},
	{39, 80},
};
#define ADVERTISMENT_CHANNELS ((int)(sizeof(g_advertisment)/sizeof(g_advertisment[0])))

static void radio_switch_channel(void)
{
	const TMapping *map;

	/* transmit beacon on all advertisment channels */
	map = &g_advertisment[g_advertisment_index++];
	if(g_advertisment_index>=ADVERTISMENT_CHANNELS)
		g_advertisment_index = 0;

	/* switch frequency & whitening */
	NRF_RADIO->FREQUENCY = map->frequency;
	NRF_RADIO->DATAWHITEIV = map->channel;
}

void RTC0_IRQ_Handler(void)
{
	/* run every two seconds */
	if(NRF_RTC0->EVENTS_COMPARE[0])
	{
		/* acknowledge event */
		NRF_RTC0->EVENTS_COMPARE[0] = 0;

		/* re-trigger timer */
		NRF_RTC0->CC[0]+= MILLISECONDS(1024);

		/* switch to next bluetooth channel */
		radio_switch_channel();
	}
}

void POWER_CLOCK_IRQ_Handler(void)
{
	/* always transmit proximity packet */
	if(NRF_CLOCK->EVENTS_HFCLKSTARTED)
	{
		/* acknowledge event */
		NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

		NRF_RADIO->EVENTS_PAYLOAD = 0;
		/* start RX */
		NRF_RADIO->TASKS_RXEN = 1;
	}
}

void RADIO_IRQ_Handler(void)
{
	/* received packet */
	if(NRF_RADIO->EVENTS_PAYLOAD)
	{
		/* acknowledge event */
		NRF_RADIO->EVENTS_PAYLOAD = 0;

		g_pkt_count++;
	}
}

int radio_packet_count(void)
{
	return g_pkt_count;
}

void radio_init(void)
{
	/* reset counters */
	g_advertisment_index = 0;
	g_pkt_count = 0;

	/* setup default radio settings for proximity mode */
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos4dBm;
	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->PREFIX0 = ((BLE_ADDRESS>>24) & RADIO_PREFIX0_AP0_Msk);
	NRF_RADIO->BASE0 = (BLE_ADDRESS<<8);
	NRF_RADIO->RXADDRESSES = 0;
	NRF_RADIO->PCNF0 =
		(1 << RADIO_PCNF0_S0LEN_Pos)|
		(8 << RADIO_PCNF0_LFLEN_Pos);
	NRF_RADIO->PCNF1 =
		(RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos)|
		(0xFF                        << RADIO_PCNF1_MAXLEN_Pos)|
		(3 << RADIO_PCNF1_BALEN_Pos);
	NRF_RADIO->CRCCNF =
		(RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) |
		(1 << RADIO_CRCCNF_SKIP_ADDR_Pos);
	NRF_RADIO->CRCINIT = 0x00555555UL;
	NRF_RADIO->CRCPOLY = 0x0100065BUL;
	NRF_RADIO->PACKETPTR = (uint32_t)&g_pkt_buffer;
	NRF_RADIO->SHORTS = (
		(RADIO_SHORTS_READY_START_Enabled       << RADIO_SHORTS_READY_START_Pos)
	);
	NRF_RADIO->INTENSET = (
		(RADIO_INTENSET_PAYLOAD_Enabled         << RADIO_INTENSET_PAYLOAD_Pos)
	);
	/* update radio channel */
	radio_switch_channel();
	/* enabled radio IRQ */
	NVIC_SetPriority(RADIO_IRQn, IRQ_PRIORITY_RADIO);
	NVIC_EnableIRQ(RADIO_IRQn);

	/* setup HF-clock IRQ */
	NRF_CLOCK->INTENSET = (
		(CLOCK_INTENSET_HFCLKSTARTED_Enabled << CLOCK_INTENSET_HFCLKSTARTED_Pos)
	);
	NVIC_SetPriority(POWER_CLOCK_IRQn, IRQ_PRIORITY_POWER_CLOCK);
	NVIC_EnableIRQ(POWER_CLOCK_IRQn);

	/* setup radio timer */
	NRF_RTC0->TASKS_STOP = 1;
	NRF_RTC0->COUNTER = 0;
	NRF_RTC0->PRESCALER = 0;
	NRF_RTC0->CC[0] = LF_FREQUENCY;
	NRF_RTC0->INTENSET = (
		(RTC_INTENSET_COMPARE0_Enabled   << RTC_INTENSET_COMPARE0_Pos)
	);
	NVIC_SetPriority(RTC0_IRQn, IRQ_PRIORITY_RTC0);
	NVIC_EnableIRQ(RTC0_IRQn);
	NRF_RTC0->TASKS_START = 1;

	/* start HF crystal oscillator */
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
}
