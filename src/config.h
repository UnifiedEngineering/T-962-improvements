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
* Normally the control input is the average of the first two TCs.
* By defining this any TC that has a readout 5C (or more) higher
* than the TC0 and TC1 average will be used as control input instead.
* Use if you have very sensitive components. Note that this will also
* kick in if the two sides of the oven has different readouts, as the
* code treats all four TCs the same way.
*/
// #define MAXTEMPOVERRIDE

/*
 * Fully dump the content of the eeprom at startup, mainly for debugging
 */
// define DUMP_EEPROM

/*
 * include a ramp test profile into the standard profiles (see reflow_profiles.c)
 */
#define RAMPTEST_PROFILE

/*
 * include a PID controller profile into the standard profiles (see reflow_profiles.c)
 */
#define PIDTEST_PROFILE

/*
 * don't use the font width information in bit 7 of the font, simply always use
 * a width of 6 pixels (see lcd.c)
 */
// #define MINIMALISTIC

/*
 * use a super-fast serial line (2Mbit/s)
 * improve speed if the transceiver works at this speed
 */
// #define SERIAL_2MBs
