/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/wait.h>

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

	// ascii h, p, or r
	int action;

	int do_clean = 1;
	int do_rcshutdown = 1;
	int do_sync  = 1;

	if (geteuid() != 0)
		errx(1, "must run as root");

	progname = malloc(strlen(argv[0]) + 2);
	if (progname == NULL)
		err(1, "malloc(3) failed");
	progname = basename(argv[0]);

	if (strcmp(progname, "halt") == 0)
		action = 'h';
	else if (strcmp(progname, "poweroff") == 0)
		action = 'p';
	else if (strcmp(progname, "reboot") == 0)
		action = 'r';
	else
		action = 'p';

	while ((ch = getopt(argc, argv, "hnpqrS")) != -1) {
		switch(ch) {
		case 'h':
			action = 'h';
			break;
		case 'n':
			do_sync = 0;
			break;
		case 'p':
			action = 'p';
			break;
		case 'q':
			do_clean = 0;
			break;
		case 'r':
			action = 'r';
			break;
		case 'S':
			do_rcshutdown = 0;
			break;
		default:
			usage();
			exit(1);
			break;
		}
	}

	// note: defined in <unistd.h> instead of <stdlib.h> in glibc
	daemon(0, 1);

	if (do_rcshutdown) {
		warnx("running /etc/rc.shutdown");

		int pid = fork();
		if (pid == -1) {
			warn("fork(2) failed");
		} else if (pid == 0) {
			if (access("/etc/rc.shutdown", X_OK) == -1) {
				warn("could not access /etc/rc.shutdown");
			} else {
				execl("/etc/rc.shutdown", "rc.shutdown", NULL);
			}
		} else {
			wait(NULL);
		}
	}

	// this is where the fun begins
	block_all_sigs();

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

	if (action == 'h') {
		warnx("halt");
		osind_reboot(OSIND_RB_HALT);
	} else if (action == 'p') {
		warnx("poweroff");
		osind_reboot(OSIND_RB_POWEROFF);
	} else if (action == 'r') {
		warnx("reboot");
		osind_reboot(OSIND_RB_REBOOT);
	} else {
		// should never happen
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
	fprintf(stderr, "usage: <halt|poweroff|reboot> [-hnpqrS]\n");
	fprintf(stderr, "    -h  halt\n");
	fprintf(stderr, "    -n  do not sync(2)\n");
	fprintf(stderr, "    -p  poweroff\n");
	fprintf(stderr, "    -q  do not give processes a chance to shut down "
	    "cleanly\n");
	fprintf(stderr, "    -r  reboot\n");
	fprintf(stderr, "    -S  do not run /etc/rc.shutdown\n");
}

int
wait_for_upto(int secs)
{
	struct timespec one_sec = { 1, 0 };

	for (int i=0; i<secs; i++) {
		nanosleep(&one_sec, NULL);

		if (kill(-1, 0) == -1) {
			// all processes have exited
			return 0;
		}
	}

	// processes remain
	return -1;
}
