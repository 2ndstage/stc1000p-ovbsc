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
 */

#define __16f1828
#include "pic14/pic16f1828.h"
#include "stc1000p.h"

#define reset() { __asm RESET __endasm; }

/* Helpful defines to handle buttons */
#define BTN_PWR				0x88
#define BTN_S				0x44
#define BTN_UP				0x22
#define BTN_DOWN			0x11

#define BTN_IDLE(btn)			((_buttons & (btn)) == 0x00)
#define BTN_PRESSED(btn)		((_buttons & (btn)) == ((btn) & 0x0f))
#define BTN_HELD(btn)			((_buttons & (btn)) == (btn))
#define BTN_RELEASED(btn)		((_buttons & (btn)) == ((btn) & 0xf0))
#define BTN_HELD_OR_RELEASED(btn)	((_buttons & (btn) & 0xf0))

/* Menu struct */
struct s_menu {
	unsigned char led_c_10;
	unsigned char led_c_1;
	unsigned char led_c_01;
	unsigned char type;
};

/* Set menu struct data generator */
#define TO_STRUCT(name, led10ch, led1ch, led01ch, type, default_value) \
    { led10ch, led1ch, led01ch, type },

static const struct s_menu menu[] = {
	MENU_DATA(TO_STRUCT)
};

static void menu_to_led(unsigned char mi){
	led_e.e_negative = 1;
	led_e.e_deg = 1;
	led_e.e_c = 1;
	led_e.e_point = 1;
	if(mi < MENU_SIZE){
		led_10.raw = menu[mi].led_c_10;
		led_1.raw = menu[mi].led_c_1;
		led_01.raw = menu[mi].led_c_01;
	} else {
		led_10.raw = LED_r;
		led_1.raw = LED_U;
		led_01.raw = LED_n;
	}
}

static int range(int x, int min, int max){
	if(x>max)
		return min;
	if(x<min)
		return max;
	return x;
}

static int check_value(unsigned char mi, int mv){
	if(mi < MENU_SIZE){
		if(menu[mi].type == t_temperature){
			mv = range(mv, MIN_TEMP, MAX_TEMP);
		} else if(menu[mi].type == t_tempdiff){
			mv = range(mv, MIN_TEMP_DIFF, MAX_TEMP_DIFF);
		} else if(menu[mi].type == t_duration){
			mv = range(mv, 0, 999);
		} else if(menu[mi].type == t_percentage){
			mv = range(mv, 0, 200);
		} else if(menu[mi].type == t_bool){
			mv = range(mv, 0, 1);
		}
	} else {
		 mv = range(mv, 0, 3);
	}
	return mv;
}

/* States for the menu FSM */
enum menu_states {
	menu_idle = 0,
	menu_show_output,
	menu_show_state,
	menu_show_countdown,
	menu_show_item,
	menu_set_item,
	menu_show_value,
	menu_set_value
};


/* Due to a fault in SDCC, static local variables are not initialized
 * properly, so the variables below were moved from button_menu_fsm()
 * and made global.
 */
static unsigned char menustate=menu_idle;
static unsigned char menu_item=0, m_countdown=0;
static int menu_value;
static unsigned char _buttons = 0;

/* This is the button input and menu handling function.
 * arguments: none
 * returns: nothing
 */
void button_menu_fsm(){
	{
		unsigned char trisc, latb;

		// Disable interrups while reading buttons
		GIE = 0;

		// Save registers that interferes with LED's
		latb = LATB;
		trisc = TRISC;

		LATB = 0b00000000; // Turn off LED's
		TRISC = 0b11011000; // Enable input for buttons

		_buttons = (_buttons << 1) | RC7; // pwr
		_buttons = (_buttons << 1) | RC4; // s
		_buttons = (_buttons << 1) | RC6; // up
		_buttons = (_buttons << 1) | RC3; // down

		// Restore registers
		LATB = latb;
		TRISC = trisc;

		// Reenable interrups
		GIE = 1;
	}

	if(m_countdown){
		m_countdown--;
	}

	switch(menustate){
		case menu_idle:
			if(ALARM && ((_buttons & 0x0f) == 0) && ((_buttons & 0xf0) !=0)){
				ALARM = 0;
			} else if(BTN_RELEASED(BTN_PWR)){
				PAUSE = !PAUSE;
			} else if(BTN_RELEASED(BTN_S)){
				menustate = menu_show_item;
			} else if(BTN_HELD(BTN_UP)){
				menustate = menu_show_output;
			} else if(BTN_HELD(BTN_DOWN)){
				m_countdown = 20;
				menustate = menu_show_state;
			}
		break;
		case menu_show_output:
			if(OFF){
				led_10.raw = LED_O;
				led_1.raw = LED_F;
				led_01.raw = LED_F;
				led_e.raw = LED_OFF;
			} else if(PAUSE){
				led_10.raw = LED_P;
				led_1.raw = LED_S;
				led_01.raw = LED_E;
				led_e.raw = LED_OFF;
			} else if(THERMOSTAT){
				temperature_to_led(setpoint);
			} else {
				int_to_led(output);
			}
			if(!BTN_HELD(BTN_UP)){
				menustate = menu_idle;
			}
		break;
		case menu_show_state:
			if(OFF){
				led_10.raw = LED_O;
				led_1.raw = LED_F;
				led_01.raw = LED_F;
				led_e.raw = LED_OFF;
			} else if(RUN_PRG){
				led_01.raw = LED_OFF;
				led_e.raw = LED_OFF;
				if(prg_state<4){ //prg_init_mash_step
					led_10.raw = LED_S;
					led_1.raw = LED_t;
				} else if(prg_state<6){ //prg_wait_boil_up_alarm
					led_10.raw = LED_P;
					led_1.raw = LED_OFF;
				} else if(prg_state<9){ //prg_boil
					led_10.raw = LED_H;
					led_1.raw = LED_b;
				} else {
					led_10.raw = LED_b;
					led_1.raw = LED_OFF;
				}
			} else if(THERMOSTAT){
				led_10.raw = LED_t;
				led_1.raw = LED_h;
				led_01.raw = LED_OFF;
				led_e.raw = LED_OFF;
			}
			if(m_countdown==0){
				m_countdown = 20;
				// prg_wait_strike,prg_mash,prg_hotbreak,prg_boil
				if(prg_state == 1 || prg_state == 5 || prg_state > 8){
					menustate = menu_show_countdown;
				}
			}
			if(!BTN_HELD(BTN_DOWN)){
				menustate = menu_idle;
			}
		break;
		case menu_show_countdown:
			int_to_led(countdown);
			if(m_countdown==0){
				m_countdown = 20;
				menustate = menu_show_state;
			}
			if(!BTN_HELD(BTN_DOWN)){
				menustate = menu_idle;
			}
		break;
		case menu_show_item:
			menu_to_led(menu_item);
			m_countdown = 110;
			menustate = menu_set_item;
		break;
		case menu_set_item:
			if(m_countdown==0 || BTN_RELEASED(BTN_PWR)){
				menustate=menu_idle;
			} else if(BTN_RELEASED(BTN_UP)){
				menu_item++;
				if(menu_item > MENU_SIZE){
					menu_item = 0;
				}
				menustate = menu_show_item;
			} else if(BTN_RELEASED(BTN_DOWN)){
				menu_item--;
				if(menu_item > MENU_SIZE){
					menu_item = MENU_SIZE;
				}
				menustate = menu_show_item;
			} else if(BTN_RELEASED(BTN_S)){
				if(menu_item < MENU_SIZE){
					menu_value = eeprom_read_config(menu_item);
				} else {
					if(OFF){
						menu_value=0;
					} else if(RUN_PRG){
						menu_value=1;
					} else if(THERMOSTAT){
						menu_value=2;
					} else {
						menu_value = 3;
					}
				}
				menustate = menu_show_value;
			}
		break;
		case menu_show_value:
			if(menu_item < MENU_SIZE){
				if(menu[menu_item].type == t_temperature || menu[menu_item].type == t_tempdiff){
					temperature_to_led(menu_value);
				} else {
					int_to_led(menu_value);
				}
			} else {
				led_e.e_negative = 1;
				led_e.e_deg = 1;
				led_e.e_c = 1;
				led_e.e_point = 1;
				if(menu_value==0){
					led_10.raw = LED_O;
					led_1.raw = LED_F;
					led_01.raw = LED_F;
				} else if(menu_value==1){
					led_10.raw = LED_P;
					led_1.raw = LED_r;
					led_01.raw = LED_OFF;
				} else if(menu_value==2){
					led_10.raw = LED_c;
					led_1.raw = LED_t;
					led_01.raw = LED_OFF;
				} else {
					led_10.raw = LED_c;
					led_1.raw = LED_O;
					led_01.raw = LED_OFF;
				}
			}
			m_countdown = 110;
			menustate = menu_set_value;
		break;
		case menu_set_value:
			if(m_countdown==0){
				menustate=menu_idle;
			} else if(BTN_RELEASED(BTN_PWR)){
				menustate = menu_show_item;
			} else if(BTN_HELD_OR_RELEASED(BTN_UP)) {
				menu_value++;
				if(menu_value > 1000){
					menu_value+=9;
				}
				/* Jump to exit code shared with BTN_DOWN case */
				goto chk_value_acc_label;
			} else if(BTN_HELD_OR_RELEASED(BTN_DOWN)) {
				menu_value--;
				if(menu_value > 1000){
					menu_value-=9;
				}
chk_value_acc_label:
				menu_value = check_value(menu_item, menu_value);
				if(PR6 > 30){
					PR6-=8;
				}
				menustate = menu_show_value;
			} else if(BTN_RELEASED(BTN_S)){
				if(menu_item < MENU_SIZE){
					eeprom_write_config(menu_item, menu_value);
				} else {
					if(menu_value==0){ // OFF
						OFF = 1;
						RUN_PRG=0;
						PUMP=0;
						THERMOSTAT = 0;
					} else if(menu_value==1){ // Pr
						OFF=0;
						RUN_PRG=1;
					} else if(menu_value==2){ // Ct
						OFF = 0;
						RUN_PRG=0;
						THERMOSTAT = 1;
					} else { // Co
						OFF = 0;
						RUN_PRG=0;
						THERMOSTAT = 0;
					}
				}
				menustate=menu_show_item;
			} else {
				PR6 = 250;
			}
		break;
		default:
			menustate = menu_idle;
		break;
	} // switch(menustate)

	MENU_IDLE = (menustate==menu_idle);
}

