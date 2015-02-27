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

#include "stc1000p.h"

/* Configuration words */
unsigned int __at _CONFIG1 __CONFIG1 = 0xFD4;
unsigned int __at _CONFIG2 __CONFIG2 = 0x3AFF;

/* Temperature lookup table  */
#ifdef FAHRENHEIT
const int ad_lookup[] = { 0, -555, -319, -167, -49, 48, 134, 211, 282, 348, 412, 474, 534, 593, 652, 711, 770, 831, 893, 957, 1025, 1096, 1172, 1253, 1343, 1444, 1559, 1694, 1860, 2078, 2397, 2987 };
#else  // CELSIUS
const int ad_lookup[] = { 0, -486, -355, -270, -205, -151, -104, -61, -21, 16, 51, 85, 119, 152, 184, 217, 250, 284, 318, 354, 391, 431, 473, 519, 569, 624, 688, 763, 856, 977, 1154, 1482 };
#endif

/* LED character lookup table (0-15), includes hex */
//unsigned const char led_lookup[] = { 0x3, 0xb7, 0xd, 0x25, 0xb1, 0x61, 0x41, 0x37, 0x1, 0x21, 0x5, 0xc1, 0xcd, 0x85, 0x9, 0x59 };
/* LED character lookup table (0-9) */
unsigned const char led_lookup[] = { LED_0, LED_1, LED_2, LED_3, LED_4, LED_5, LED_6, LED_7, LED_8, LED_9 };

/* Global variables to hold LED data (for multiplexing purposes) */
led_e_t led_e = {0xff};
led_t led_10, led_1, led_01;
led_t al_led_10, al_led_1, al_led_01;

int setpoint=0;
static int temperature=0;

unsigned char output=0;
static unsigned char thermostat_output=0;


/* Functions.
 * Note: Functions used from other page cannot be static, but functions
 * not used from other page SHOULD be static to decrease overhead.
 * Functions SHOULD be defined before used (ie. not just declared), to
 * decrease overhead. Refer to SDCC manual for more info.
 */

/* Read one configuration data from specified address.
 * arguments: Config address (0-127)
 * return: the read data
 */
unsigned int eeprom_read_config(unsigned char eeprom_address){
	unsigned int data = 0;
	eeprom_address = (eeprom_address << 1);

	do {
		EEADRL = eeprom_address; // Data Memory Address to read
		CFGS = 0; // Deselect config space
		EEPGD = 0; // Point to DATA memory
		RD = 1; // Enable read

		data = ((((unsigned int) EEDATL) << 8) | (data >> 8));
	} while(!(eeprom_address++ & 0x1));

	return data; // Return data
}

/* Store one configuration data to the specified address.
 * arguments: Config address (0-127), data
 * return: nothing
 */
void eeprom_write_config(unsigned char eeprom_address,unsigned int data)
{
	// Avoid unnecessary EEPROM writes
	if(data == eeprom_read_config(eeprom_address)){
		return;
	}

	// multiply address by 2 to get eeprom address, as we will be storing 2 bytes.
	eeprom_address = (eeprom_address << 1);

	do {
		// Address to write
	    EEADRL = eeprom_address;
	    // Data to write
	    EEDATL = (unsigned char) data;
	    // Deselect configuration space
	    CFGS = 0;
	    //Point to DATA memory
	    EEPGD = 0;
	    // Enable write
	    WREN = 1;

	    // Disable interrupts during write
	    GIE = 0;

	    // Write magic words to EECON2
	    EECON2 = 0x55;
	    EECON2 = 0xAA;

	    // Initiate a write cycle
	    WR = 1;

	    // Re-enable interrupts
	    GIE = 1;

	    // Disable writes
	    WREN = 0;

	    // Wait for write to complete
	    while(WR);

	    // Clear write complete flag (not really needed
	    // as we use WR for check, but is nice)
	    EEIF=0;

	    // Shift data for next pass
	    data = data >> 8;

	} while(!(eeprom_address++ & 0x01)); // Run twice for 16 bits

}

static unsigned int divu10(unsigned int n) {
	unsigned int q, r;
	q = (n >> 1) + (n >> 2);
	q = q + (q >> 4);
	q = q + (q >> 8);
	q = q >> 3;
	r = n - ((q << 3) + (q << 1));
	return q + ((r + 6) >> 4);
}

/* Update LED globals with temperature or integer data.
 * arguments: value (actual temperature multiplied by 10 or an integer)
 *            decimal indicates if the value is multiplied by 10 (i.e. a temperature)
 * return: nothing
 */
void value_to_led(int value, unsigned char decimal) {
	unsigned char i;

	// Handle negative values
	if (value < 0) {
		led_e.e_negative = 0;
		value = -value;
	} else {
		led_e.e_negative = 1;
	}

	// This assumes that only temperatures and all temperatures are decimal
	if(decimal){
		led_e.e_deg = 0;
#ifdef FAHRENHEIT
		led_e.e_c = 1;
#else
		led_e.e_c = 0;
#endif // FAHRENHEIT
	}

	// If temperature >= 100 we must lose decimal...
	if (value >= 1000) {
		value = divu10((unsigned int) value);
		decimal = 0;
	}

	// Convert value to BCD and set LED outputs
	if(value >= 100){
		for(i=0; value >= 100; i++){
			value -= 100;
		}
		led_10.raw = led_lookup[i & 0xf];
	} else {
		led_10.raw = LED_OFF; // Turn off led if zero (lose leading zeros)
	}
	if(value >= 10 || decimal || led_10.raw!=LED_OFF){ // If decimal, we want 1 leading zero
		for(i=0; value >= 10; i++){
			value -= 10;
		}
		led_1.raw = led_lookup[i];
		if(decimal){
			led_1.decimal = 0;
		}
	} else {
		led_1.raw = LED_OFF; // Turn off led if zero (lose leading zeros)
	}
	led_01.raw = led_lookup[(unsigned char)value];
}

static unsigned char output_counter=0;
static void output_control(){
	static unsigned char o;
	
	if(output_counter == 0){
		output_counter = 100;
		o = output;
	}
	output_counter--;

	LATA0 = 0;
	led_e.e_point = 1;
	if(ALARM){
		LATA0 = (output_counter > 75);
	} else if(PAUSE && !OFF){
		led_e.e_point = output_counter & 0x1;
	}

	if(PAUSE || OFF){
		LATA5 = 0;
		LATA4 = 0;
		PUMP_OFF();
	} else {

		LATA5 = (o > output_counter);
		LATA4 = (o > (output_counter+100));

		if(PUMP){
			PUMP_MANUAL();
		} else {
			PUMP_OFF();
		}

	}

	led_e.e_heat = !LATA5;
	led_e.e_cool = !LATA4;
}

static void temperature_control(){
	if(THERMOSTAT){
		if(temperature < setpoint){
			output = thermostat_output;
		} else {
			output = 0;
		}
	}
}

unsigned char prg_state=0;
unsigned int countdown=0;
unsigned char mashstep=0;
static void program_fsm(){
	static unsigned char sec_countdown=60;

	if(OFF){
		prg_state = prg_off;
		return;
	}

	if(PAUSE){
		return;
	}

	if(countdown){
		if(sec_countdown==0){
			sec_countdown=60;
			countdown--;
		}
		sec_countdown--;
	}

	switch(prg_state){
		case prg_off:
			if(RUN_PRG){
				countdown = eeprom_read_config(EEADR_MENU_ITEM(Sd)); 	/* Strike delay */
				prg_state = prg_wait_strike;
			} else {
				if(THERMOSTAT){
					setpoint = eeprom_read_config(EEADR_MENU_ITEM(cSP));
					thermostat_output = eeprom_read_config(EEADR_MENU_ITEM(cO));
				} else {
					output = eeprom_read_config(EEADR_MENU_ITEM(cO));
				}
				PUMP = eeprom_read_config(EEADR_MENU_ITEM(cP));
			}
		break;
		case prg_wait_strike:
			output = 0;
			THERMOSTAT = 0;
			PUMP=0;
			if(countdown == 0){
				countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
				prg_state = prg_strike;
			}
		break;
		case prg_strike:
			setpoint = eeprom_read_config(EEADR_MENU_ITEM(St)); /* Strike temp */
			output = eeprom_read_config(EEADR_MENU_ITEM(SO));
			PUMP=1;
			if(temperature >= setpoint){
				THERMOSTAT = 1;
				thermostat_output = eeprom_read_config(EEADR_MENU_ITEM(PO));
				ALARM = 1;
				al_led_10.raw = LED_S;
				al_led_1.raw = LED_t;
				al_led_01.raw = LED_OFF;
				countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
				prg_state = prg_strike_wait_alarm;
			} else if(countdown == 0){
				OFF=1;
			}
		break;
		case prg_strike_wait_alarm:
			if(!ALARM){
				PAUSE = 1;
				mashstep=0;
				countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
				prg_state = prg_init_mash_step;
			} else if(countdown == 0){
				OFF=1;
			}
		break;
		case prg_init_mash_step:
			setpoint = eeprom_read_config(EEADR_MENU_ITEM(Pt1) + (mashstep << 1)); /* Mash step temp */
			output = eeprom_read_config(EEADR_MENU_ITEM(SO));
			THERMOSTAT = 0;
			PUMP = 1;
			if(temperature >= setpoint){
				THERMOSTAT = 1;
				thermostat_output = eeprom_read_config(EEADR_MENU_ITEM(PO));
				countdown = eeprom_read_config(EEADR_MENU_ITEM(Pd1) + (mashstep << 1)); /* Mash step duration */
				prg_state = prg_mash;
			} else if(countdown==0){
				OFF=1;
			}
		break;
		case prg_mash:
			if(countdown==0){
				mashstep++;
				if(mashstep < 4){
					countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
					prg_state = prg_init_mash_step;
				} else {
					ALARM = 1;
					al_led_10.raw = LED_b;
					al_led_1.raw = LED_U;
					al_led_01.raw = LED_OFF;
					countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
					prg_state = prg_wait_boil_up_alarm;
				}
			}
		break;
		case prg_wait_boil_up_alarm:
			if(!ALARM){
				PAUSE=1;
				countdown = eeprom_read_config(EEADR_MENU_ITEM(ASd));
				prg_state = prg_init_boil_up;
			} else if(countdown == 0){
				OFF=1;
			}
		break;
		case prg_init_boil_up:
			THERMOSTAT = 0;
			PUMP = 0;
			output = eeprom_read_config(EEADR_MENU_ITEM(SO)); /* Boil output */
			if(temperature >= eeprom_read_config(EEADR_MENU_ITEM(Ht))){ /* Boil up temp */
				countdown = eeprom_read_config(EEADR_MENU_ITEM(Hd)); /* Hotbreak duration */
				prg_state = prg_hotbreak;
			} else if(countdown==0){
				OFF=1;
			}
		break;
		case prg_hotbreak:
			output = eeprom_read_config(EEADR_MENU_ITEM(HO)); /* Hot break up output */
			if(countdown==0){
				countdown = eeprom_read_config(EEADR_MENU_ITEM(bd)); /* Boil duration */ 
				prg_state = prg_boil;
			}
		break;
		case prg_boil:
			output = eeprom_read_config(EEADR_MENU_ITEM(bO)); /* Boil output */
			{
				unsigned char i;
				for(i=0; i<4; i++){
					if(countdown == eeprom_read_config(EEADR_MENU_ITEM(hd1) + i) && sec_countdown > 57){ /* Hop timer */
						ALARM = 1;
						al_led_10.raw = LED_h;
						al_led_1.raw = LED_d;
						al_led_01.raw = led_lookup[i+1];
						break;
					}
				}
			}
			if(countdown == 0){
				output = 0;
				THERMOSTAT = 0;
				OFF = 1;
				ALARM = 1;
				al_led_10.raw = LED_C;
				al_led_1.raw = LED_h;
				al_led_01.raw = LED_OFF;
				RUN_PRG = 0;
				prg_state = prg_off;
			}
		break;

	} // switch(prg_state)

}



/* Initialize hardware etc, on startup.
 * arguments: none
 * returns: nothing
 */
static void init() {

//   OSCCON = 0b01100010; // 2MHz
	OSCCON = 0b01101010; // 4MHz

	// Heat, cool as output, Thermistor as input, piezo output
	TRISA = 0b00001100;
	LATA = 0; // Drive relays and piezo low

	// Make sure weak pullup is disabled for RA1
	WPUA1 = 0;

	// LED Common anodes
	TRISB = 0;
	LATB = 0;

	// LED data (and buttons) output
	TRISC = 0;

	// Analog input on thermistor
	ANSELA = _ANSA1;
	// AD clock FOSC/8 (FOSC = 4MHz)
	ADCS0 = 1;
	// Right justify AD result
	ADFM = 1;
	// Enable AD
	ADCON0 = _CHS1 | _ADON;
	// Start conversion
	ADGO = 1;

	// IMPORTANT FOR BUTTONS TO WORK!!! Disable analog input -> enables digital input
	ANSELC = 0;

	// Postscaler 1:1, Enable counter, prescaler 1:4
	T2CON = 0b00000101;
	// @4MHz, Timer 2 clock is FOSC/4 -> 1MHz prescale 1:4-> 250kHz, 250 gives interrupt every 1 ms
	PR2 = 250;
	// Enable Timer2 interrupt
	TMR2IE = 1;

#ifdef T1MAIN
	//Timer1 Registers Prescaler= 1 - TMR1 Preset = 3036 - Freq = 16.00 Hz - Period = 0.062500 seconds
	// T1CON
	TMR1CS1 = 0;
	TMR1CS0 = 0;
//	T1CKPS1 = 1;   // bits 5-4  Prescaler Rate Select bits
//	T1CKPS0 = 1;   // bit 4
	T1CKPS1 = 0;   // bits 5-4  Prescaler Rate Select bits
	T1CKPS0 = 0;   // bit 4
	T1OSCEN = 0;   // bit 3 Timer1 Oscillator Enable Control bit 1 = on
	NOT_T1SYNC = 1;    // bit 2 Timer1 External Clock Input Synchronization Control bit...1 = Do not synchronize external clock input
//	TMR1CS = 0;    // bit 1 Timer1 Clock Source Select bit...0 = Internal clock (FOSC/4)
	TMR1ON = 1;    // bit 0 enables timer
//	TMR1H = 11;             // preset for timer1 MSB register
//	TMR1L = 220;             // preset for timer1 LSB register

//	TMR1IE = 1;	// Enable interrupts

	// CCP4 in special event trigger mode
	// TODO Datasheet is ambigous, needs to be ECCP1 or CCP4?
	CCPR4H = 0xF4;
	CCPR4L = 0x24;
	CCP4CON = 0xB;
#else
	// Postscaler 1:16, - , prescaler 1:16
	T4CON = 0b01111010;
	TMR4ON = 1; // eeprom_read_config(EEADR_POWER_ON);
	// @4MHz, Timer 2 clock is FOSC/4 -> 1MHz prescale 1:16-> 62.5kHz, 244 and postscale 1:16 -> 16.01 Hz or about 62.5ms
	PR4 = 244;
#endif

	// Postscaler 1:7, Enable counter, prescaler 1:64
	T6CON = 0b00110111;
	// @4MHz, Timer 2 clock is FOSC/4 -> 1MHz prescale 1:64-> 15.625kHz, 250 and postscale 1:6 -> 8.93Hz or 112ms
	PR6 = 250;

	// Set PEIE (enable peripheral interrupts, that is for timer2) and GIE (enable global interrupts)
	INTCON = 0b11000000;

}

/* Interrupt service routine.
 * Receives timer2 interrupts every millisecond.
 * Handles multiplexing of the LEDs.
 */
static void interrupt_service_routine(void) __interrupt 0 {

	// Check for Timer 2 interrupt
	// Kind of excessive when it's the only enabled interrupt
	// but is nice as reference if more interrupts should be needed
	if (TMR2IF) {
		unsigned char latb = (LATB << 1);

		if(latb == 0){
			latb = 0x10;
		}

		TRISC = 0; // Ensure LED data pins are outputs
		LATB = 0; // Disable LED's while switching

		// Multiplex LED's every millisecond
		switch(latb) {
			case 0x10:
			LATC = led_10.raw;
			break;
			case 0x20:
			LATC = led_1.raw;
			break;
			case 0x40:
			LATC = led_01.raw;
			break;
			case 0x80:
			LATC = led_e.raw;
			break;
		}

		// Enable new LED
		LATB = latb;

		// Clear interrupt flag
		TMR2IF = 0;
	}
}

/*
 * Main entry point.
 */
void main(void) __naked {
	unsigned char cnt16Hz=0;
	unsigned int ad_filter=0;

	init();

	//Loop forever
	while (1) {

		if(TMR6IF) {

			// Handle button press and menu
			button_menu_fsm();

#ifdef T1MAIN
			if(!TMR1ON){
#else
			if(!TMR4ON){
#endif				led_e.raw = LED_OFF;
				led_10.raw = LED_O;
				led_1.raw = led_01.raw = LED_F;
			}

			// Reset timer flag
			TMR6IF = 0;
		}

#ifdef T1MAIN
		if(CCP4IF) {
#else
		if(TMR4IF) {
#endif
			cnt16Hz++;

			output_control();

			// Only run every 16th time called, that is 16x62.5ms = 1 sec
			if((cnt16Hz & 0xf) == 0) {

				/* Convert AD readings to temperature */
				{
					unsigned char i;
					long temp = 32;
					unsigned char a = ((ad_filter >> 3) & 0x3f); // Lower 6 bits
					unsigned char b = ((ad_filter >> 9) & 0x1f); // Upper 5 bits

					// Interpolate between lookup table points
					for (i = 0; i < 64; i++) {
						if(a <= i) {
							temp += ad_lookup[b];
						} else {
							temp += ad_lookup[b + 1];
						}
					}

					// Divide by 64 to get back to normal temperature
					temperature = (temp >> 6);
				}
				
				ad_filter = 0;

				program_fsm();
				temperature_control();

				if(MENU_IDLE){
					if(ALARM && (cnt16Hz & 0x10)){
						led_10.raw = al_led_10.raw;
						led_1.raw = al_led_1.raw;
						led_01.raw = al_led_01.raw;
					} else {
						temperature_to_led(temperature);
					}
				}

			} // End 1 sec section

#ifdef T1MAIN
			while(ADGO);
			ad_filter += ((ADRESH << 8) | ADRESL);
			CCP4IF = 0;
#else
			ad_filter += ((ADRESH << 8) | ADRESL);
			ADGO = 1;
			TMR4IF = 0;
#endif
		}

		// Reset watchdog
		ClrWdt();
	}
}
