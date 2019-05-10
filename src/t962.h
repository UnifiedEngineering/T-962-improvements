#ifndef T962_H_
#define T962_H_

//////////// some helpers
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
// using attribute unused is strange with something like 'char *argv[]'
#define UNUSED(a) ((void)(a))

// trap value within [min, max]
static inline int coerce(int value, int min, int max)
{
	if (value > max)
		return max;
	if (value < min)
		return min;

	return value;
}

// wrap value within [min, max]
static inline int wrap(int value, int min, int max)
{
	if (value > max)
		return min;
	if (value < min)
		return max;

	return value;
}

typedef enum eMainMode {
	MAIN_HOME = 0,
	MAIN_ABOUT,
	MAIN_SETUP,
	MAIN_BAKE_SETUP,
	MAIN_BAKE,
	MAIN_SELECT_PROFILE,
	MAIN_EDIT_PROFILE,
	MAIN_REFLOW_SETUP,
	MAIN_REFLOW,
	MAIN_COOLING
} MainMode_t;

int Set_Mode(MainMode_t);

/* Hardware notes and pin mapping:
 *
 * LCD is KS0108 compatible, with two chip selects, active high
 * EEPROM is an AT24C02C with 2kbits capacity (256x8)
 * XTAL frequency is 11.0592MHz
 *
 * GPIO0 port
 * 0.0 ISP/Console UART TX output
 * 0.1 ISP/Console UART RX input
 * 0.2 EEPROM SCL line (SCL0) w/ 4k7 pullup
 * 0.3 EEPROM SDA line (SDA0) w/ 4k7 pullup
 * 0.4 F4 button (active low, with pullup)
 * 0.5 ?
 * 0.6 ?
 * 0.7 Used for 1-wire cold-junction DS18B20 temperature sensor retrofit
 * 0.8 Fan output (active low) PWM4
 * 0.9 Heater output (active low) PWM6
 * 0.10 ?
 * 0.11 LCD backlight control output (active high)
 * 0.12 LCD CS2 output (right side of display)
 * 0.13 LCD CS1 output (left side of display)
 * 0.14 Enter ISP mode if low during reset (4k7 pullup)
 * 0.15 F2 button (active low, with pullup)
 * 0.16 F3 button (active low, with pullup)
 * 0.17 ?
 * 0.18 LCD E(nable) output (data latched on falling edge)
 * 0.19 LCD RW output (0=write, 1=read)
 * 0.20 S button (active low, with pullup)
 * 0.21 Buzzer output (active high) PWM5
 * 0.22 LCD RS output (0=cmd, 1=data)
 * 0.23 F1 button (active low, with pullup) VBUS input as alt function on LPC214x
 * 0.24 (No pin)
 * 0.25 AD0.4 accessible on test point marked ADO - DAC output as alt function (now used for sysfan control)
 * 0.26 ? USB D+ on LPC214x-series of chips
 * 0.27 ? USB D- on LPC214x-series of chips
 * 0.28 Temperature sensor input 1 (AD0.1), this was the left-hand one in our oven
 * 0.29 Temperature sensor input 2 (AD0.2), this was the right-hand one in our oven
 * 0.30 ?
 * 0.31 Red LED on board (active low)
 *
 * GPIO1 port
 * 1.16-1.23 LCD D0-D7 I/O
 * 1.24 ?
 * 1.25 ?
 * 1.26 ?
 * 1.27 ?
 * 1.28 ?
 * 1.29 ?
 * 1.30 ?
 * 1.31 ?
 */

/* EEPROM contents:
 *   The original EEPROM content as specified here, is actually of no interest anymore.
 *   The shell 'format' command can be used to preset the EEPROM with valid setting.
 */

#define PCLKFREQ (5 * 11059200)

#endif /* T962_H_ */
