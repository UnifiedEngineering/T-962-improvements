/*
 * config.h - configuration for several builds of the T-962 reflow controller
 *
 * Copyright (C) 2017 Helmut Raiger helmut.raiger@gmx.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * this is a patch that drives a mica heating element placed
 * underneath the PCB to be soldered. To use it an additional
 * PWM channel is required, currently the only one left is used
 * by the cold junction compensation patch, but this can be
 * moved to P0.5 (pin 29), which is moved by this define!
 */
#define USE_SECONDARY_HEATER

/*
 * if the 74hc04 is replaced by 2 FETs to make sure the Opto-TRIAC
 * receives the correct amount of drive current, the drive logic is
 * inverted, i.e. high means TRIAC on. This changes the logic in io.c
 */
#define USE_FET_DRIVER

/*
 * use this to adjust gain and omit offset compensation settings
 * in sensor.c and setup.c
 * This needs a precision operational amplifier (like an OPA2333)
 * at a rather precise gain of 222.2 on the TC input!
 * The feedback resistors are 220k + 1.2k and 1.0k with 0.1% tolerance
 */
#define USE_PRECISION_OPAMP

/*
 * The software deals with up-to 4 temperatures, configure their
 * association with the names 'left', 'right', 'extra1' and 'extra2'.
 * They need to be one of TC_ONE_WIRE, TC_SPI_BRIDGE, TC_INTERNAL or TC_NONE
 * and a channel after the comma (may be left away, defaults to 0)
 *
 * Note: the original software falls back to internal when no other
 *  interface works, this mechanism is gone now. The configuration MUST
 *  work. On the other hand if the internal TC were not connected, this
 *  led to an error even in the original version.
 */
#define TC_LEFT_IF		TC_INTERNAL, 0
#define TC_RIGHT_IF		TC_INTERNAL, 1
#define TC_EXTRA1_IF	TC_NONE
#define TC_EXTRA2_IF	TC_NONE
// the CJ compensation sensor, internal means the DS18B20 (or alike), no channel!
#define COLD_JUNCTION_IF	TC_INTERNAL

/*
 * Define the control strategy:
 *   LR_AVERAGE ... left and  right TC average is used (even if undefined)
 *
 *   MAX_TEMP_OVERRIDE:
 *     By defining this any TC that has a readout 5C (or more) higher
 *     than the TC0 and TC1 average will be used as control input instead.
 *     Use if you have very sensitive components. Note that this will also
 *     kick in if the two sides of the oven has different readouts, as the
 *     code treats all four TCs the same way.
 *
 *  LR_WEIGHTED_AVERAGE:
 *     This includes code to allow the user to specify a measure for the
 *     thermal capacity of the PCB to be soldered. This weight is
 *     used to create a weighted average of the left and right thermocouple.
 *     This is ONLY useful if the TCs are patched to carry such loads, i.e.
 *     a 5c Euro coin on the right thermo-couple and 0.2mm copper foil on the left.
 */
#define CONTROL_TEMPERATURE		LR_WEIGHTED_AVERAGE

/*
 * Fully dump the content of the eeprom at startup, mainly for debugging
 */
// define DUMP_EEPROM

/*
 * include a ramp test profile into the standard profiles (see reflow_profiles.c)
 */
#define RAMPTEST_PROFILE

/*
 * don't use the font width information in bit 7 of the font, simply always use
 * a width of 6 pixels (see lcd.c)
 */
// #define MINIMALISTIC


/*
 *  serial baudrate:
 *  allowed values 2000842 (special), 115200, 57600, 38400, 19200, 9600, 4800
 */
#define UART0_BAUDRATE	115200
