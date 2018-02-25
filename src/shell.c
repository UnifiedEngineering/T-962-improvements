#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "serial.h"
#include "sched.h"
#include "log.h"
#include "io.h"
#include "sensor.h"
#include "eeprom.h"
#include "version.h"
#include "reflow_profiles.h"
#include "nvstorage.h"
#include "setup.h"
#include "reflow.h"

#include "SimpleCLI/inc/scliCore.h"

// some color shortcuts
#define RED			"\x1b[01;31m"
#define GREEN		"\x1b[01;32m"
#define YELLOW		"\x1b[01;33m"
#define BLUE		"\x1b[01;34m"
#define MAGENTA		"\x1b[01;35m"
#define CYAN		"\x1b[01;36m"
// actually the foreground color might as well be black
#define WHITE		"\x1b[0m"
#define WHITE_BOLD	"\x1b[01;30m"


#if 0

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

			} else if (strcmp(serial_cmd, "reflow") == 0) {
				printf("\nStarting reflow with profile: %s\n", Reflow_GetProfileName());
				mode = MAIN_HOME;
				// this is a bit dirty, but with the least code duplication.
				keyspressed = KEY_S;

			} else if (strcmp(serial_cmd, "stop") == 0) {
				printf("\nStopping bake/reflow");
				mode = MAIN_HOME;
				Reflow_SetMode(REFLOW_STANDBY);
				retval = 0;

			} else if (strcmp(serial_cmd, "quiet") == 0) {
				Reflow_ToggleStandbyLogging();
				printf("\nToggled standby logging\n");


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

			} else if (sscanf(serial_cmd, cmd_setting, &param, &paramF) > 0) {
				Setup_setRealValue(param, paramF);
				printf("\nAdjusted setting: ");
				Setup_printFormattedValue(param);
			}
		}
	}
}
#endif

SCLI_CMD_RET cmd_info(uint8_t argc, char *argv[])
{
	char buf[40];

	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\nT-962 build version: %s\n", Version_GetGitVersion());
	IO_Partinfo(buf, sizeof(buf), "Part number: %s rev %c\n\n");
	printf(buf);
	EEPROM_Dump();

	printf("\n\nSensor values:\n");
	Sensor_ListAll();

	return 0;
}

SCLI_CMD_RET cmd_profiles(uint8_t argc, char *argv[])
{
	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\n");
	Reflow_ListProfiles();

	return 0;
}

SCLI_CMD_RET cmd_settings(uint8_t argc, char *argv[])
{
	char buf[40];

	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\nCurrent settings:\n\n");
	for (int i = 0; i < Setup_getNumItems() ; i++) {
		printf("%d: ", i);
		Setup_snprintFormattedValue(buf, sizeof(buf), i);
		printf("%s\n", buf);
	}

	return 0;
}

SCLI_CMD_RET cmd_dump(uint8_t argc, char *argv[])
{
	if (argc < 2) {
		// no \n shell reports error aswell
		printf(RED "\n ... dump what?" WHITE);
		return -1;
	}

	if (strcmp(argv[1], "profile") == 0) {
		int no = Reflow_GetProfileIdx();
		if (argc > 2) {
			no = atoi(argv[2]);
		}
		printf("\nProfile (%d) '%s'\n", no, Reflow_GetProfileName(no));
		Reflow_DumpProfile(no);
	} else if (strcmp(argv[1], "eeprom") == 0) {
		EEPROM_Dump();
	} else {
		printf(RED "\n ... don't know how to dump '%s'" WHITE, argv[1]);
		return -1;
	}

	return 0;
}

SCLI_CMD_RET cmd_set(uint8_t argc, char *argv[])
{
	int value;

	if (argc < 2) {
		// no \n shell reports error aswell
		printf(RED "\n ... set what?" WHITE);
		return -1;
	}

	if (strcmp(argv[1], "log_lvl") == 0) {
		value = 0;
		if (argc > 2) {
			value = atoi(argv[2]);
		}
		set_log_level(value);
	} else if (strcmp(argv[1], "reflow_log_lvl") == 0) {
		value = 0;
		if (argc > 2) {
			value = atoi(argv[2]);
		}
		Reflow_SetLogLevel(value);
	} else {
		printf(RED "\n ... don't know how to set '%s'" WHITE, argv[1]);
		return -1;
	}

	printf("\n%s is now set to %d\n", argv[1], value);

	return 0;
}


SCLI_CMD_T commands[] = {
		{ cmd_info, "info", "some system information",
				"show version, CPU information and more" },
		{ cmd_profiles, "profiles", "list profiles",
				"list profiles" },
		{ cmd_settings, "settings", "list settings",
				"list settings" },
		{ cmd_dump, "dump", "dump [profile|eeprom|...] [nr]",
				"dump profile [no] .. with number 'no' or the currently selected\n"
				"dump eeprom .. show eeprom dump\n"
		},
		{ cmd_set, "set", "set var [value]",
				"set 'var' to a 'value' or default\n"
				"  var = [log_lvl, reflow_log_lvl]"
		},

		SCLI_CMD_LIST_END
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
	// turn off debug logging on shell startup
	set_log_level(LOG_WARN);
}

// get a character if any and process it
int32_t Shell_Work(void) {
	if (uart_isrxready() == 0)
		return TICKS_MS(100);

	uint8_t c = (uint8_t) uart_readc();
	if (char_handler)
		char_handler(c);

	// return immediately if characters are flowing in
	return 0;
}
