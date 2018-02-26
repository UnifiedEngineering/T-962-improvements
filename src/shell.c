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

SCLI_CMD_RET cmd_info(uint8_t argc, char *argv[])
{
	char buf[40];

	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\nT-962 build version: %s\n", Version_GetGitVersion());
	IO_Partinfo(buf, sizeof(buf), "Part number: %s rev %c\n");
	printf(buf);
	printf("\nSensor values:");
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

// use it like:
// save my_profile 50,50,50,50,55,70,85,90,95,100,102,105,107,110,112,115,117,120,122,127,135,140,145,150,160,170,175,170,160,150,140,130,120
SCLI_CMD_RET cmd_save(uint8_t argc, char *argv[])
{
	int idx;
	char *name;

	if (argc < 2 || argc > 3) {
		printf(RED "\n ... need name and possibly some values (no spaces!)");
		return -1;
	}

	idx = Reflow_GetProfileIdx();
	if (!Reflow_IdxIsInEEPROM(idx)) {
		printf(RED "\n ... selected index %d is not editable (in EEPROM)", idx);
		return -1;
	}

	name = argv[1];
	if (strcmp(name, "''") == 0) {
		printf(YELLOW "\nsetting name to default 'CUSTOM #'" WHITE);
		name = "";
	}
	Reflow_SetProfileName(idx, name);

	if (argc == 3) {
		static char buffer[SCLI_CMD_MAX_LEN];
	    char *p;
	    // keep argv intact
		strcpy(buffer, argv[2]);

		p = strtok(buffer, ",");
		for (int idx=0; idx < 48 && p; idx++) {
			Reflow_SetSetpointAtIdx(idx, (uint16_t) atoi(p));
			p = strtok (NULL, ",");
		}
	}

	return Reflow_SaveEEProfile();
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
		if (argc > 2)
			value = atoi(argv[2]);
		set_log_level(value);
	} else if (strcmp(argv[1], "reflow_log_lvl") == 0) {
		value = 0;
		if (argc > 2)
			value = atoi(argv[2]);
		Reflow_SetLogLevel(value);
	} else if (strcmp(argv[1], "profile") == 0) {
		value = Reflow_GetProfileIdx();
		// test if MAIN_HOME, but don't switch anywhere
		if (Set_Mode(MAIN_HOME)) {
			if (argc > 2)
				value = atoi(argv[2]);
			value = Reflow_SelectProfileIdx(value);
			printf("\nProfile (%d) '%s'\n", value, Reflow_GetProfileName(value));
		} else {
			printf(RED "\n switch to main screen via keyboard first!" WHITE);
			return -1;
		}
	} else {
		printf(RED "\n ... don't know how to set '%s'" WHITE, argv[1]);
		return -1;
	}

	printf("\n%s is now set to %d\n", argv[1], value);

	return 0;
}

SCLI_CMD_RET cmd_reflow(uint8_t argc, char *argv[])
{
	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\n");
	if (!Set_Mode(MAIN_REFLOW)) {
		printf(RED "\n switch to main screen via keyboard first!" WHITE);
		return -1;
	}
	printf("\nStart reflow with profile: %s\n", Reflow_GetProfileName(-1));

	return 0;
}

SCLI_CMD_RET cmd_bake(uint8_t argc, char *argv[])
{
	if (argc < 2) {
		printf(RED "\n need setpoint value" WHITE);
		return -1;
	}
	int setpoint = coerce(atoi(argv[1]), SETPOINT_MIN, SETPOINT_MAX);

	int bake_timer = BAKE_TIMER_MAX;
	if (argc > 2) {
		// needs to be at least 1, otherwise it ends immediately
		bake_timer = coerce(atoi(argv[2]), 1, BAKE_TIMER_MAX);
	}

	printf("\n");
	if (!Set_Mode(MAIN_BAKE)) {
		printf(RED "\n switch to main screen via keyboard first!" WHITE);
		return -1;
	}

	Reflow_SetSetpoint(setpoint);
	Reflow_SetBakeTimer(bake_timer);
	printf("\nStart bake at %ddegC, for %ds after setpoint reached\n", setpoint, bake_timer);

	return 0;
}

SCLI_CMD_RET cmd_abort(uint8_t argc, char *argv[])
{
	if (argc > 1)
		printf(YELLOW "\n... ignoring arguments\n" WHITE);

	printf("\n");
	if (!Set_Mode(MAIN_HOME)) {
		printf(RED "\n only BAKE or REFLOW mode may be aborted" WHITE);
		return -1;
	}

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
				"dump eeprom .. show eeprom dump\n" },
		{ cmd_set, "set", "set var [value]",
				"set 'var' to a 'value' or default\n"
				"  var is one of: log_lvl reflow_log_lvl profile" },
		{ cmd_reflow, "reflow", "start reflow", "start reflow" },
		{ cmd_bake, "bake", "bake setpoint [timer], start bake mode",
				"start bake at setpoint degC, and time in seconds if given (default = max)" },
		{ cmd_abort, "abort", "abort reflow or bake mode",
				"abort mode and go to main menu, this stops an ongoing bake or reflow" },
		{ cmd_save, "save", "profile_name [upto 48 values], stores profile data",
				"store a profile at current profile with name and values\n"
				"values must be comma separated (no spaces!), like '17,18,19,20,0'" },
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
