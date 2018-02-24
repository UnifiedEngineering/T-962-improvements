#include <stdint.h>
#include <stdio.h>

#include "serial.h"
#include "sched.h"
#include "log.h"
#include "SimpleCLI/inc/scliCore.h"

#if 0
static char* format_about = \
"\nT-962-controller open source firmware (%s)" \
"\n" \
"\nSee https://github.com/UnifiedEngineering/T-962-improvement for more details." \
"\n" \
"\nInitializing improved reflow oven...";

static char* help_text = \
"\nT-962-controller serial interface.\n\n" \
" about                   Show about + debug information\n" \
" bake <setpoint>         Enter Bake mode with setpoint\n" \
" bake <setpoint> <time>  Enter Bake mode with setpoint for <time> seconds\n" \
" help                    Display help text\n" \
" list profiles           List available reflow profiles\n" \
" list settings           List machine settings\n" \
" quiet                   No logging in standby mode\n" \
" reflow                  Start reflow with selected profile\n" \
" setting <id> <value>    Set setting id to value\n" \
" select profile <id>     Select reflow profile by id\n" \
" stop                    Exit reflow or bake mode\n" \
" values                  Dump currently measured values\n" \
"\n";

/*
 * this is simply stripped out of Main_Work() to simplify
 *   replacement.
 */
static void minimal_shell(void) {
	char serial_cmd[255] = "";
	char* cmd_select_profile = "select profile %d";
	char* cmd_bake = "bake %d %d";
	char* cmd_dump_profile = "dump profile %d";
	char* cmd_setting = "setting %d %f";

	if (uart_isrxready()) {
		int len = uart_readline(serial_cmd, 255);

		if (len > 0) {
			int param, param1;
			float paramF;

			if (strcmp(serial_cmd, "about") == 0) {
				printf(format_about, Version_GetGitVersion());
				len = IO_Partinfo(buf, sizeof(buf), "\nPart number: %s rev %c\n");
				printf(buf);
				EEPROM_Dump();

				printf("\nSensor values:\n");
				Sensor_ListAll();

			} else if (strcmp(serial_cmd, "help") == 0 || strcmp(serial_cmd, "?") == 0) {
				printf(help_text);

			} else if (strcmp(serial_cmd, "list profiles") == 0) {
				printf("\nReflow profiles available:\n");

				Reflow_ListProfiles();
				printf("\n");

			} else if (strcmp(serial_cmd, "reflow") == 0) {
				printf("\nStarting reflow with profile: %s\n", Reflow_GetProfileName());
				mode = MAIN_HOME;
				// this is a bit dirty, but with the least code duplication.
				keyspressed = KEY_S;

			} else if (strcmp(serial_cmd, "list settings") == 0) {
				printf("\nCurrent settings:\n\n");
				for (int i = 0; i < Setup_getNumItems() ; i++) {
					printf("%d: ", i);
					Setup_printFormattedValue(i);
					printf("\n");
				}

			} else if (strcmp(serial_cmd, "stop") == 0) {
				printf("\nStopping bake/reflow");
				mode = MAIN_HOME;
				Reflow_SetMode(REFLOW_STANDBY);
				retval = 0;

			} else if (strcmp(serial_cmd, "quiet") == 0) {
				Reflow_ToggleStandbyLogging();
				printf("\nToggled standby logging\n");

			} else if (strcmp(serial_cmd, "values") == 0) {
				printf("\nActual measured values:\n");
				Sensor_ListAll();
				printf("\n");

			} else if (sscanf(serial_cmd, cmd_select_profile, &param) > 0) {
				// select profile
				Reflow_SelectProfileIdx(param);
				printf("\nSelected profile %d: %s\n", param, Reflow_GetProfileName());

			} else if (sscanf(serial_cmd, cmd_bake, &param, &param1) > 0) {
				if (param < SETPOINT_MIN) {
					printf("\nSetpoint must be >= %ddegC\n", SETPOINT_MIN);
					param = SETPOINT_MIN;
				}
				if (param > SETPOINT_MAX) {
					printf("\nSetpont must be <= %ddegC\n", SETPOINT_MAX);
					param = SETPOINT_MAX;
				}
				if (param1 < 1) {
					printf("\nTimer must be greater than 0\n");
					param1 = 1;
				}

				if (param1 < BAKE_TIMER_MAX) {
					printf("\nStarting bake with setpoint %ddegC for %ds after reaching setpoint\n", param, param1);
					timer = param1;
					Reflow_SetBakeTimer(timer);
				} else {
					printf("\nStarting bake with setpoint %ddegC\n", param);
				}

				setpoint = param;
				Reflow_SetSetpoint(setpoint);
				mode = MAIN_BAKE;
				Reflow_SetMode(REFLOW_BAKE);

			} else if (sscanf(serial_cmd, cmd_dump_profile, &param) > 0) {
				printf("\nDumping profile %d: %s\n ", param, Reflow_GetProfileName());
				Reflow_DumpProfile(param);

			} else if (sscanf(serial_cmd, cmd_setting, &param, &paramF) > 0) {
				Setup_setRealValue(param, paramF);
				printf("\nAdjusted setting: ");
				Setup_printFormattedValue(param);

			} else {
				printf("\nUnknown command '%s', ? for help\n", serial_cmd);
			}
		}
	}
}
#endif

SCLI_CMD_RET cmd_about(uint8_t argc, char *argv[])
{
	printf("about");
	return 0;
}

SCLI_CMD_T commands[] = {
		{ cmd_about, "info", "show basic information", "" }
};

// being cooperative no critical zone management necessary
static uint32_t critical_enter(void) { return 1; }
static void critical_exit(uint32_t reg) { (void) reg; }
// output from shell (at least if not printf() is used ???)
static void uart_putchar(char ch) { _write(0, &ch, 1); }
// callback from SimpleCLI if '\r' is received, no idea why that is necessary?
static void user_handover(SCLI_CMD_INPUT_T *input) {
	scliCore_ExecuteCommand(input);
}
// during scliCore_Init this is called with the character handler function
// install it locally
static void (*char_handler)(uint8_t) = 0;
static void uart_system_init(void (*f)(uint8_t )) {
	char_handler = f;
}

SCLI_INTERFACE_T shell_if = {
		.SubsystemInit = uart_system_init,
		.PutCh = uart_putchar,
		.UserLevelHandOver = user_handover,
		.CriticalEnter = critical_enter,
		.CriticalExit = critical_exit,
		.UserLevelCommands = commands
};

void Shell_Init(void) {
	scliCore_Init(&shell_if);
}

// get a character if any and process it
int32_t Shell_Work(void) {
	if (uart_isrxready() == 0)
		return TICKS_MS(100);

	uint8_t c = (uint8_t) uart_readc();
	if (char_handler)
		char_handler(c);

	return TICKS_MS(100);
}
