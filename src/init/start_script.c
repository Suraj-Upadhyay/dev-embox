/**
 * @file
 * @brief Runs start script
 *
 * @date 04.10.11
 * @author Alexander Kalmuk
 */

#include <errno.h>
#include <util/array.h>
#include <embox/unit.h>
#include <framework/cmd/api.h>
#include <cmd/shell.h>
#include <stdio.h>

#define START_SHELL OPTION_GET(NUMBER,shell_start)

EMBOX_UNIT_INIT(run_script);

static const char *script_commands[] = {
	#include <start_script.inc>
};

extern int setup_tty(const char *dev_name);

static int run_script(void) {
	const char *command;
	const struct shell *shell;

	shell = shell_lookup(OPTION_STRING_GET(shell_name));
	if (NULL == shell) {
		shell = shell_any();
		if (NULL == shell) {
			return -ENOENT;
		}
	}

	setup_tty(OPTION_STRING_GET(tty_dev));

	printf("\nStarted shell [%s] on device [%s]\n",
		OPTION_STRING_GET(shell_name), OPTION_STRING_GET(tty_dev));

	printf("loading start script:\n");
	array_foreach(command, script_commands, ARRAY_SIZE(script_commands)) {
		printf("> %s \n", command);

		shell_exec(shell, command);
	}

	shell_run(shell);

	return 0;
}
