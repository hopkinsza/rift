/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <getopt.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "rfconf.h"

#define RC_SHUTDOWN ETCDIR "/rc.shutdown"

/*
 * Use 4.4BSD-like logwtmp(3) from <util.h>.
 *
 * Linux (glibc) has this in <utmp.h>.
 */

#ifdef BSD
#if defined(USE_UTMP) || defined(USE_UTMPX)
#include <util.h>
#endif
#endif

#ifdef __linux__
#ifdef USE_UTMP
#include <utmp.h>
#endif
#endif

#include "osind_reboot.h"

#ifndef HPR_GRACE
#define HPR_GRACE 5
#endif

int wait_for_upto(int);
void block_all_sigs();
void usage();

int
main(int argc, char *argv[])
{
	char *progname;
	char *user;

	// ascii h, p, or r
	int action;

	int do_clean = 1;
	int do_log = 1;
	int do_rcshutdown = 1;
	int do_sync  = 1;

	if (geteuid() != 0)
		errx(1, "must run as root");

	// try to grab a username for later logging
	user = getlogin();
	if (user == NULL) {
		struct passwd *p;
		p = getpwuid(getuid());

		if (p == NULL) {
			user = "???";
		} else {
			user = p->pw_name;
		}
	}

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

	int ch;
	while ((ch = getopt(argc, argv, "hlnpqrS")) != -1) {
		switch(ch) {
		case 'h':
			action = 'h';
			break;
		case 'l':
			do_log = 0;
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
			break;
		}
	}

	// note: defined in <unistd.h> instead of <stdlib.h> in glibc
	daemon(0, 1);

	if (do_rcshutdown) {
		warnx("running " RC_SHUTDOWN);

		int pid = fork();
		if (pid == -1) {
			warn("fork(2) failed");
		} else if (pid == 0) {
			if (access(RC_SHUTDOWN, X_OK) == -1) {
				warn("could not access " RC_SHUTDOWN);
			} else {
				execl(RC_SHUTDOWN, "rc.shutdown", NULL);
			}
		} else {
			wait(NULL);
		}
	}

	if (do_log) {
		openlog(progname, LOG_CONS, LOG_AUTH);
		switch (action) {
		case 'h':
			syslog(LOG_CRIT, "halted by %s", user);
			break;
		case 'p':
			syslog(LOG_CRIT, "powered off by %s", user);
			break;
		case 'r':
			syslog(LOG_CRIT, "rebooted by %s", user);
			break;
		}
		closelog();
	}

	/*
	 * Write wtmp/wtmpx record.
	 */

#ifdef USE_UTMP
	logwtmp("~", "shutdown", "");
#endif
#ifdef USE_UTMPX
	// NetBSD extension
	logwtmpx("~", "shutdown", 0, INIT_PROCESS);
#endif

	// this is where the fun begins
	block_all_sigs();
	kill(1, SIGTSTP);

	if (do_clean) {
		warnx("waiting up to %d seconds for processes to exit...",
		    HPR_GRACE);
		kill(-1, SIGTERM);
		kill(-1, SIGHUP);
		kill(-1, SIGCONT);
		wait_for_upto(HPR_GRACE);
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
	fprintf(stderr, "usage: <halt|poweroff|reboot> [-hlnpqrS]\n");
	fprintf(stderr, "    -h  halt\n");
	fprintf(stderr, "    -l  do not log to syslog(3)\n");
	fprintf(stderr, "    -n  do not sync(2)\n");
	fprintf(stderr, "    -p  poweroff\n");
	fprintf(stderr, "    -q  do not give processes a chance to shut down "
	    "cleanly\n");
	fprintf(stderr, "    -r  reboot\n");
	fprintf(stderr, "    -S  do not run " RC_SHUTDOWN "\n");
	exit(1);
}
