/*
 * scliText.c
 *
 *  Created on: 31.01.2016
 *      Author: Matthias Beckert <beckert.matthias@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

const char scliText_ColRedBold[] = "\x1b[01;31m";
const char scliText_ColGreBold[] = "\x1b[01;32m";
const char scliText_ColYelBold[] = "\x1b[01;33m";
const char scliText_ColBluBold[] = "\x1b[01;34m";
const char scliText_ColMagBold[] = "\x1b[01;35m";
const char scliText_ColCyaBold[] = "\x1b[01;36m";
const char scliText_ColDef[] =     "\x1b[0m";
const char scliText_ColDefBold[] = "\x1b[01;30m";
const char scliText_DeleteRight[]= "\x1b[K";

const char scliText_Error[] =
    "";

const char scliText_WelcomeMsg[] =
    " SimpleCLI V0.1\r\n \"I aim to misbehave!\"\r\n";

const char scliText_Prompt[] =
    " > ";

const char scliText_Help[] =
    " This is the help function\r\n  help <cmd>";

const char scliText_Version[] =
    " SimpleCLI V0.1\r\n"
    " Author Mattias Beckert\r\n"
    "             <beckert.matthias@googlemail.com>";

const char scliText_Set[] =
    " Set a system variable to a given value\r\n  set <ident> <value>";

const char scliText_Get[] =
    " Get value of a system variable\r\n  get <ident>";

const char scliText_Conf[] =
    " Manage configuration subsystem\r\n";

const char scliText_ConfNoTable[] =
    " No config table found, system might be uninitialized\r\n"
    " scliConf_RegisterConfig(...) must be called before at least once!\r\n";
