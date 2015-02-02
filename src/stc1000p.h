/*
 * STC1000+, improved firmware and Arduino based firmware uploader for the STC-1000 dual stage thermostat.
 *
 * Copyright 2014 Mats Staffansson
 *
 * This file is part of STC1000+.
 *
 * STC1000+ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * STC1000+ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with STC1000+.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Schematic of the connections to the MCU.
 *
 *                                     PIC16F1828
 *                                    ------------
 *                                VDD | 1     20 | VSS
 *                     Relay Heat RA5 | 2     19 | RA0/ICSPDAT (Programming header), Piezo buzzer
 *                     Relay Cool RA4 | 3     18 | RA1/AN1/ICSPCLK (Programming header), Thermistor
 * (Programming header) nMCLR/VPP/RA3 | 4     17 | RA2/AN2 Thermistor
 *                          LED 5 RC5 | 5     16 | RC0 LED 0
 *                   LED 4, BTN 4 RC4 | 6     15 | RC1 LED 1
 *                   LED 3, BTN 3 RC3 | 7     14 | RC2 LED 2
 *                   LED 6, BTN 2 RC6 | 8     13 | RB4 LED Common Anode 10's digit
 *                   LED 7, BTN 1 RC7 | 9     12 | RB5 LED Common Anode 1's digit
 *        LED Common Anode extras RB7 | 10    11 | RB6 LED Common Anode 0.1's digit
 *                                    ------------
 *
 *
 * Schematic of the bit numbers for the display LED's. Useful if custom characters are needed.
 *
 *           * 7       --------    *    --------       * C
 *                    /   7   /    1   /   7   /       5 2
 *                 2 /       / 6    2 /       / 6    ----
 *                   -------          -------     2 / 7 / 6
 *           *     /   1   /        /   1   /       ---
 *           3  5 /       / 3    5 /       / 3  5 / 1 / 3
 *                --------    *    --------   *   ----  *
 *                  4         0      4        0    4    0
 *
 *
 *
 *
 */

#ifndef __STC1000P_H__
#define __STC1000P_H__

#define __16f1828
#include "pic14/pic16f1828.h"

/* Define STC-1000+ version number (XYY, X=major, YY=minor) */
/* Also, keep track of last version that has changes in EEPROM layout */
#define STC1000P_VERSION		(100)
#define STC1000P_EEPROM_VERSION		(10)

/* Clear Watchdog */
#define ClrWdt() 			{ __asm CLRWDT __endasm; }

/* Special registers used as flags */ 
#define	MENU_IDLE			TMR1GE
#define	SENSOR_SELECT			RX9
#define RUN_PRG				C1POL
#define ALARM				C2POL
#define	PAUSE				C1HYS
#define THERMOSTAT			C2HYS
#define PUMP				C1SYNC
#define UNUSED2				C2SYNC
/* Initialized to 1 */
#define OFF				C1SP

#define	PUMP_ON()			do { TRISA1=1; LATA1=1; } while(0)
#define PUMP_OFF()			do { TRISA1=1; LATA1=0; } while(0)
#define PUMP_MANUAL()			do { TRISA1=0; LATA1=0; } while(0)

/* Defaults */
#ifdef FAHRENHEIT
// TODO Sane values...
#define DEFAULT_St			(690)
#define DEFAULT_mt			(670)
#else  // CELSIUS
#define DEFAULT_St			(345)
#define DEFAULT_mt			(345)
#endif

/* Limits */
#ifdef FAHRENHEIT
// TODO Sane values...
#define MIN_TEMP			(-400)
#define MAX_TEMP			(2500)
#define MIN_TEMP_DIFF			(-100)
#define MAX_TEMP_DIFF			(100)
#else  // CELSIUS
#define MIN_TEMP			(-400)
#define MAX_TEMP			(1400)
#define MIN_TEMP_DIFF			(-50)
#define MAX_TEMP_DIFF			(50)
#endif

enum e_item_type {
	t_temperature=0,
	t_tempdiff,
	t_duration,
	t_percentage,
	t_bool
};

#define MENU_DATA(_) \
    _(Sd, 	LED_S, 	LED_d, 	LED_OFF,	t_duration,	0) 		\
    _(St, 	LED_S, 	LED_t, 	LED_OFF, 	t_temperature,	DEFAULT_St)	\
    _(SO, 	LED_S, 	LED_O, 	LED_OFF,	t_percentage,	200)		\
    _(Pt1, 	LED_P, 	LED_t, 	LED_1,		t_temperature,	DEFAULT_mt)	\
    _(Pd1, 	LED_P, 	LED_d, 	LED_1,		t_duration,	15)		\
    _(Pt2, 	LED_P, 	LED_t, 	LED_2,		t_temperature,	DEFAULT_mt)	\
    _(Pd2, 	LED_P, 	LED_d, 	LED_2,		t_duration,	30)		\
    _(Pt3, 	LED_P, 	LED_t, 	LED_3,	 	t_temperature,	DEFAULT_mt)	\
    _(Pd3, 	LED_P, 	LED_d, 	LED_3,		t_duration,	15)		\
    _(Pt4, 	LED_P, 	LED_t, 	LED_4,	 	t_temperature,	DEFAULT_mt)	\
    _(Pd4, 	LED_P, 	LED_d, 	LED_4,		t_duration,	0)		\
    _(PO, 	LED_P, 	LED_O, 	LED_OFF,	t_percentage,	50)		\
    _(Ht, 	LED_H, 	LED_t, 	LED_OFF,	t_temperature,	985)		\
    _(HO, 	LED_H, 	LED_O, 	LED_OFF,	t_percentage,	0)		\
    _(Hd, 	LED_H, 	LED_d, 	LED_OFF,	t_duration,	0)		\
    _(bO, 	LED_b, 	LED_O, 	LED_OFF,	t_percentage,	0)		\
    _(bd, 	LED_b, 	LED_d, 	LED_OFF,	t_duration,	0)		\
    _(hd1, 	LED_h, 	LED_d, 	LED_1,		t_duration,	60)		\
    _(hd2, 	LED_h, 	LED_d, 	LED_2,		t_duration,	45)		\
    _(hd3, 	LED_h, 	LED_d, 	LED_3, 		t_duration,	15)		\
    _(hd4, 	LED_h, 	LED_d, 	LED_4,		t_duration,	5)		\
    _(tc, 	LED_t, 	LED_c, 	LED_OFF,	t_tempdiff,	0)		\
    _(cO, 	LED_c, 	LED_O, 	LED_OFF,	t_percentage,	80)		\
    _(cP, 	LED_c, 	LED_P, 	LED_OFF,	t_bool,		0)		\
    _(cSP, 	LED_c, 	LED_S, 	LED_P, 		t_temperature,	0)		\
    _(ASd, 	LED_A, 	LED_S, 	LED_d, 		t_duration,	90)		\



#define ENUM_VALUES(name, led10ch, led1ch, led01ch, type, default_value) \
    name,

/* Generate enum values for each entry int the set menu */
enum menu_enum {
    MENU_DATA(ENUM_VALUES)
};

/* Defines for EEPROM config addresses */
#define EEADR_MENU_ITEM(name)		(name)

#define MENU_SIZE			(sizeof(menu)/sizeof(menu[0]))

#define LED_OFF	0xff
#define LED_0	0x3
#define LED_1	0xb7
#define LED_2	0xd
#define LED_3	0x25
#define LED_4	0xb1
#define LED_5	0x61
#define LED_6	0x41
#define LED_7	0x37
#define LED_8	0x1
#define LED_9	0x21
#define LED_A	0x11
#define LED_a	0x5
#define LED_b	0xc1
#define LED_C	0x4b
#define LED_c	0xcd
#define LED_d	0x85
#define LED_e	0x9
#define LED_E	0x49
#define LED_F	0x59
#define LED_H	0x91
#define LED_h	0xd1
#define LED_I	0xb7
#define LED_J	0x87
#define LED_L	0xcb
#define LED_n	0xd5	
#define LED_O	0x3
#define LED_P	0x19
#define LED_r	0xdd	
#define LED_S	0x61
#define LED_t	0xc9
#define LED_U	0x83
#define LED_y	0xa1

/* Declare functions and variables from Page 0 */

typedef union
{
	unsigned char raw;

	struct
	  {
	  unsigned                      : 1;
	  unsigned e_point              : 1;
	  unsigned e_c                  : 1;
	  unsigned e_heat               : 1;
	  unsigned e_negative           : 1;
	  unsigned e_deg                : 1;
	  unsigned e_set                : 1;
	  unsigned e_cool               : 1;
	  };
} led_e_t;

typedef union
{
	unsigned char raw;

	struct
	  {
	  unsigned decimal		: 1;
	  unsigned middle		: 1;
	  unsigned upper_left		: 1;
	  unsigned lower_right          : 1;
	  unsigned bottom		: 1;
	  unsigned lower_left		: 1;
	  unsigned upper_right		: 1;
	  unsigned top			: 1;
	  };
} led_t;

extern led_e_t led_e;
extern led_t led_10, led_1, led_01;
extern unsigned const char led_lookup[];

extern int setpoint;
extern unsigned char output;

extern unsigned char prg_state;
extern unsigned int countdown;


extern unsigned int eeprom_read_config(unsigned char eeprom_address);
extern void eeprom_write_config(unsigned char eeprom_address,unsigned int data);
extern void value_to_led(int value, unsigned char decimal);
#define int_to_led(v)		value_to_led(v, 0);
#define temperature_to_led(v)	value_to_led(v, 1);

/* Declare functions and variables from Page 1 */
extern void button_menu_fsm();

#endif // __STC1000P_H__
