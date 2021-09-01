/*
 * SPDX-License-Identifier: 0BSD
 */

#include <err.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "osind_reboot.h"

void block_all_sigs();
void usage();
int wait_for_upto(int);

int
main(int argc, char *argv[])
{
	char *progname;
	int ch;

	int do_halt     = 0;
	int do_poweroff = 0;
	int do_reboot   = 0;

	int do_clean = 1;
	int do_sync  = 1;

	block_all_sigs();

	if (geteuid() != 0)
		errx(1, "must run as root");

	progname = malloc(strlen(argv[0]) + 2);
	if (progname == NULL)
		err(1, "malloc(3) failed");
	progname = basename(argv[0]);

	if (strcmp(progname, "poweroff") == 0)
		do_poweroff = 1;
	else if (strcmp(progname, "reboot") == 0)
		do_reboot = 1;
	else
		do_halt = 1;

	while ((ch = getopt(argc, argv, "npq")) != -1) {
		switch(ch) {
		case 'n':
			do_sync = 0;
			break;
		case 'p':
			if (do_halt) {
				do_halt = 0;
				do_poweroff = 1;
			}
			break;
		case 'q':
			do_clean = 0;
			break;
		default:
			usage();
			exit(1);
			break;
		}
	}

	if (do_clean) {
		warnx("waiting up to 4 seconds for processes to exit...");
		kill(-1, SIGTERM);
		kill(-1, SIGHUP);
		kill(-1, SIGCONT);
		wait_for_upto(4);
	}

	if (do_sync) {
		warnx("sync()");
		sync();
	}

	if (do_halt) {
		warnx("halt");
		osind_reboot(OSIND_RB_HALT);
	} else if (do_poweroff) {
		warnx("poweroff");
		osind_reboot(OSIND_RB_POWEROFF);
	} else if (do_reboot) {
		warnx("reboot");
		osind_reboot(OSIND_RB_REBOOT);
	} else {
		/* should never happen */
		abort();
	}
}


void
block_all_sigs()
{
	sigset_t bmask;

	sigfillset(&bmask);
	sigprocmask(SIG_BLOCK, &bmask, NULL);
}

void
usage()
{
	fprintf(stderr, "usage: <halt|poweroff|reboot> [-npq]\n");
	fprintf(stderr, "    -n  do not sync(2)\n");
	fprintf(stderr, "    -p  if called as `halt': poweroff\n");
	fprintf(stderr, "    -q  do not give processes a chance to shut down "
	    "cleanly\n");
}

int
wait_for_upto(int secs)
{
	struct timespec one_sec = { 1, 0 };

	for (int i=0; i<secs; i++) {
		nanosleep(&one_sec, NULL);

		if (kill(-1, 0) == -1) {
			/* All processes have exited. */
			return 0;
		}
	}

	/* Processes remain. */
	return -1;
}
